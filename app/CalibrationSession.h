// One run of the automatic calibration (2.1, "Einmessen"): the state machine
// behind the wizard, the capture thread that cuts the input into takes, and the
// analysis and export jobs.
//
//     intro -> silence -> take(i) <-> stepDone -> ... -> analysing -> result (A/B) -> saved
//
// "Abbrechen" from any state ends the draft, stops the capture and frees the
// takes; nothing is written before "Speichern". Everything the session needs
// from the app goes through CalibrationHost (implemented by MainComponent).
//
// Message thread, apart from the capture thread inside.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "Calibration.h"

#include <functional>
#include <vector>

/** The values "Auf alle Presets anwenden" replaced, per preset, for "Zuruecksetzen". */
struct PresetUndo
{
    struct Entry
    {
        int slot = 0;
        juce::NamedValueSet previous;   ///< PresetStore::patch's `previous`
    };

    std::vector<Entry> entries;
    bool empty() const noexcept   { return entries.empty(); }
};

/** What the calibration needs from the app. MainComponent implements it. */
class CalibrationHost
{
public:
    virtual ~CalibrationHost() = default;

    // --- what the wizard starts from
    virtual const vm::Settings& savedEngine() const = 0;   ///< the engine settings as saved (= live; the wizard is modal)
    virtual float savedDeadZone() const = 0;
    virtual bool  cameraDelivering() const = 0;            ///< switched on and frames arriving
    virtual juce::String profileName() const = 0;          ///< the active instrument profile
    virtual int   presetSlot() const = 0;                  ///< 0 = none loaded
    virtual juce::String presetLabel() const = 0;          ///< "003 - Ballade" (with an en dash)
    virtual AudioHost& audio() = 0;
    virtual juce::String audioDescription() const = 0;     ///< device, input channel, rate, buffer, output device

    // --- remembered by the app (AppSettings::calibrationMode, lastCalibration)
    virtual int  calibrationMode() const = 0;              ///< 0 Normalmodus, 1 Profimodus
    virtual void setCalibrationMode (int) = 0;
    virtual juce::String lastCalibration() const = 0;      ///< JSON of the last saved calibration, empty = none
    virtual void setLastCalibration (const juce::String& json) = 0;

    // --- while the wizard is open
    /** Blocks MIDI program changes, the calibrated remote params, preset and profile changes by
        MIDI and camera. */
    virtual void setCalibrating (bool) = 0;
    /** Live, never to disk: the engine, the controls and an open or docked camera panel
        (CameraPanel::setDeadZone) show these values; saveSettings() keeps writing the saved ones. */
    virtual void showDraft (const vm::calib::DetectionValues&, const vm::calib::SoundValues&) = 0;
    /** The live values back to the saved ones. */
    virtual void endDraft() = 0;
    /** The live state (the draft shown) becomes the saved one: the first step of "Speichern",
        before the presets, saveSound and saveDetection (critique 2.7). */
    virtual void commitDraft() = 0;

    // --- saving (message thread); each returns false and leaves everything as it was on failure
    /** Into the active profile and the settings file (old keys). */
    virtual bool saveDetection (const vm::calib::DetectionValues&) = 0;
    /** The current preset (PresetStore::patch) and the settings file. */
    virtual bool saveSound (const vm::calib::SoundValues&) = 0;
    /** Every preset (critique 2.11): with `measuredVoices` > 0 the level is adapted to each preset's
        voice count and clamped to -24 .. +6; with 0 the values are copied as they are (the
        harmonizer window). The old values go into `undo`. */
    virtual bool applySoundToAllPresets (const vm::calib::SoundValues&, int measuredVoices, PresetUndo& undo) = 0;
    virtual bool undoPresets (const PresetUndo&) = 0;
};

class CalibrationSession final
{
public:
    enum class State { intro, silence, take, stepDone, analysing, result, saved, closed };
    enum class Mode { normal = 0, professional = 1 };
    enum class TakeState { open, current, done, skipped };
    /** Canned pages for the layout checks (--show-calibration, -step, -result). */
    enum class Demo { intro, step, result };

    explicit CalibrationSession (CalibrationHost&);
    /** Disarms the capture, joins the threads, cancels the analysis and ends the draft if nothing
        was saved. MainComponent destroys the session before the audio callback goes. */
    ~CalibrationSession();

    /** Something the wizard shows changed (state, progress, the analysis). */
    std::function<void()> onChange;
    /** The window should close: after abort(), or "Fertig" after saving. */
    std::function<void()> onClose;

    State state() const noexcept   { return current; }

    // --- intro
    void setMode (Mode);
    Mode mode() const noexcept     { return chosenMode; }
    /** 4 .. 7 strings (the intro's switch); the protocol follows. */
    void setStrings (int count);
    const vm::calib::Instrument& instrument() const noexcept   { return strings; }
    /** The count came from the factory "Tiefster Ton" (7 strings): the intro asks. */
    bool stringsFromFactoryValue() const noexcept;
    /** Why "Start" is disabled (no input, the device does not run); empty if it can start. */
    juce::String startProblem() const;
    void start();

    // --- steps
    const std::vector<vm::calib::TakeId>& protocol() const noexcept   { return takes; }
    /** Index into protocol(), -1 before the start. */
    int currentTake() const noexcept;
    TakeState takeState (int index) const noexcept;
    /** Of the current take. */
    vm::calib::Progress progress() const;
    /** The amber line under the progress: the take's hint or a device restart; empty if none. */
    juce::String hint() const;
    /** "Weiter": every string of the current step is done or skipped. */
    bool canGoOn() const noexcept;
    void next();
    void repeatCurrent();
    void skipCurrent();
    /** "Diese Saite neu aufnehmen": a take that is done, again. */
    void redo (int takeIndex);

    // --- analysis
    float analysisProgress() const noexcept;    ///< 0..1
    int   analysisPhase() const noexcept;       ///< 0 detection, 1 harmonizer, 2 report

    // --- result
    /** Null until the analysis has ended. */
    const vm::calib::Result* result() const noexcept;
    /** "Vorher | Nachher": live A/B through CalibrationHost::showDraft. */
    void showSide (bool after);
    bool showingAfter() const noexcept;
    /** "Speichern" (critique 2.7 order); `allPresets`: the latch "Auf alle Presets anwenden". */
    bool save (bool allPresets);
    /** "Zuruecksetzen": before saving the same as showSide (false); after saving it reverts the save. */
    bool revert();
    bool isSaved() const noexcept;
    /** Profimodus: "Diagnose-Ordner behalten" (default off, critique 2.13). */
    void setKeepReport (bool);
    bool keepReport() const noexcept;
    /** The diagnosis folder, empty while it is not written (yet). */
    juce::String reportFolder() const;

    /** "Abbrechen" on any page before saving: the draft goes, nothing stays. */
    void abort();
    /** The window's close button and ESC: abort() before saving, "Fertig" after. */
    void close();

    // --- from the main window
    /** Once per timer tick, the snapshot MainComponent took (one takeMeters() per tick, critique 2.6). */
    void liveMeters (const AudioHost::Meters&);
    const AudioHost::Meters& meters() const noexcept   { return lastMeters; }
    /** Every camera frame while a take runs (PadTracker::lastOffset, flickHeld). */
    void cameraSample (double timeSec, bool valid, float dx, float dy, bool flick);

    // --- layout checks
    void showDemo (Demo);

private:
    CalibrationHost& host;
    State current = State::intro;
    Mode chosenMode = Mode::normal;
    vm::calib::Instrument strings;
    std::vector<vm::calib::TakeId> takes;
    AudioHost::Meters lastMeters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CalibrationSession)
};
