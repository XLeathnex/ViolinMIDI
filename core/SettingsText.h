// ViolinMIDI core - every engine setting with its name on disk, in one table.
//
// The settings file and the presets (app/SettingsFields.cpp) and the plain
// "key=value" text of the calibration report and the tools (toText / fromText)
// are all written from this table, so they cannot drift apart.
//
// Adding an engine setting: one line in the table in SettingsText.cpp. Its
// scope says whether a preset brings its value along or the live one stays.
// To make it controllable by MIDI as well, add one entry in app/Params.cpp.
#pragma once

#include "Curve.h"
#include "Types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vm
{
namespace settingsfile
{
    struct Field
    {
        enum class Type { Flag, Whole, Number };

        /** What loading a preset does with the setting (2.1). */
        enum class Scope
        {
            preset,    ///< the preset's value is used
            profile,   ///< part of the instrument profile: the live value stays
            wiring     ///< part of how the rig is wired (dryKill): the live value stays
        };

        const char* key = "";   ///< never changes: it is the name in the settings file and in the presets
        Type   type = Type::Number;
        double low = 0.0, high = 1.0;
        std::function<double (const Settings&)> get;
        std::function<void (Settings&, double)> set;   ///< not clamped: callers limit the value to low .. high
        Scope  scope = Scope::preset;
    };

    /** Every setting that is stored as one plain value, in the order of the file. Not in it:
        the master switch and the freeze buttons (the app always starts listening, with nothing
        frozen), what the app fills in itself (the chord keyboard, the camera's bow assist), and
        the voice masks and curves, which are written by hand (below). */
    const std::vector<Field>& table();

    /** The row with this key, or null. */
    const Field* find (const std::string& key);

    /** The keys of one scope, in table order (Scope::profile: the 13 of the instrument profile). */
    std::vector<std::string> keysOf (Field::Scope);

    // The settings that are not plain numbers.
    constexpr const char* kFreeVoicing     = "freeVoicing";      ///< the semitone distances, e.g. "-12 0 7"
    constexpr const char* kVoicingVoices   = "voicingVoices";    ///< since 2.0, the same for the preset voicing as played
    constexpr const char* kVoicingVoicesOf = "voicingVoicesOf";  ///< since 2.0, the voicing those voices were made for (not in presets)
    constexpr const char* kFormantCurve    = "formantCurve";     ///< since 2.0, "note:value" pairs, e.g. "55.00:0.0500 69.00:0.2000"
    constexpr const char* kHiCutCurve      = "hiCutCurve";       ///< since 2.0, "note:Hz" pairs

    struct CurveField
    {
        const char* key;
        Curve Settings::* member;
        float low, high;
        Curve (*fallback)();
    };

    /** The formant and the high-cut curve. */
    const std::vector<CurveField>& curves();

    /** The semitone distances of a voice mask (bit semitones + 24), ascending. */
    std::vector<int> voiceList (uint64_t mask);
    /** The mask of these distances, cleaned (scale::cleanFreeMask); out-of-range ones are dropped. */
    uint64_t voiceMask (const std::vector<int>& semitones);

    /** "-12 0 7", as the settings file writes a voice list. */
    std::string voicesText (uint64_t mask);
    /** The mask of such a list; tokens that are not whole numbers are skipped. */
    uint64_t voicesFromText (const std::string&);

    /** "55.00:0.0500 69.00:0.2000", as the settings file writes a curve. */
    std::string curveText (const Curve&);
    /** The curve of such a text, sanitised to the field's range (its fallback if nothing is left). */
    Curve curveFromText (const std::string&, const CurveField&);

    /** What has to hold between fields, whatever was read: voicing and masks cleaned, curves
        sanitised, Out never above In, the pentatonic switch of 1.3 turned into its scale. The app
        adds the snap of the bend range to the ranges it offers. */
    void tidy (Settings&);

    /** 2.1: "Ton halten" (pentaSustain) works with every filter. Data written before 2.1 (it has
        no "harmonySource") may have it switched on without effect for a seven-note scale: there
        it stays without effect. Called after tidy() by every reader of settings data. */
    void migrateHold (bool writtenBefore21, Settings&) noexcept;

    /** "# VioLogic engine settings\nkey=value\n...": every table key (numbers with 9 significant
        digits, flags 0/1), the voice lists with their voicing and the curves: the engine part of
        the settings file as text. Independent of the C locale. */
    std::string toText (const Settings&);

    /** Applies every known key of such a text, clamped as the settings file is read, with the
        same rules for data of older versions; then tidy() and migrateHold(). Keys it does not know
        go into `unknown`. False if a line was not "key=value" or a known value was not a number
        (the other lines are still applied). */
    bool fromText (const std::string&, Settings& into, std::vector<std::string>* unknown = nullptr);
}

/** The preset rule of 2.1: a loaded preset takes the live value of every setting whose scope is
    not `preset`, i.e. the 13 values of the instrument profile and the wiring (dryKill). */
void keepOnPresetLoad (Settings& loaded, const Settings& live);

} // namespace vm
