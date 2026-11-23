// ViolinMIDI core - turns the per-hop pitch/level stream into MIDI.
//
// The violin-specific decisions live here:
//   * a finger change under one bow is a sudden pitch step  -> new note (legato)
//   * a finger sliding on the string is a continuous change -> pitch bend
//   * a new bow stroke on the same pitch (level dips, then rises) -> new note
//   * vibrato can be passed on, filtered out, or ignored (three bend modes)
//   * bow loudness is followed continuously with a controller (CC11, CC1 or CC2)
//   * velocity can also reflect how the note began (plucked or bowed)
#pragma once

#include "Types.h"

#include <vector>

namespace vm
{

class NoteTracker
{
public:
    struct Input
    {
        bool  voiced  = false;
        float pitch   = 0.0f;     ///< fractional MIDI note, valid when voiced
        float clarity = 0.0f;
        float hintClarity = 0.0f; ///< periodicity still present at the sounding note's period
        bool  octaveDoubt = false; ///< the detector suspects an overtone
        float levelDb = -120.0f;  ///< RMS level of the filtered input
    };

    void prepare (double hopsPerSecond);

    /** Forgets everything without sending MIDI. */
    void reset() noexcept;

    /** Call once per analysis hop. Appends events at the given sample offset. */
    void process (const Input& in, const Settings& settings, int sampleOffset, MidiEventList& out);

    /** Ends the sounding note and returns the controllers to neutral. */
    void shutdown (int sampleOffset, MidiEventList& out);

    bool  isActive() const noexcept      { return active; }
    int   note() const noexcept          { return curNote; }
    float trackedPitch() const noexcept  { return lastPitch; }
    float outputPitch() const noexcept   { return outPitch; }
    /** True while the detector should hold on to the sounding note's period. */
    bool  wantsHint() const noexcept     { return active && ! inTransition; }

private:
    void applySettings (const Settings& s, int offset, MidiEventList& out);
    enum class ExprUpdate { Follow, Raise, Seed };
    void updateExpression (float levelDb, int offset, MidiEventList& out, ExprUpdate kind);
    void handBackControllers (unsigned mask, int offset, MidiEventList& out);
    void processIdle (const Input& in, int offset, MidiEventList& out);
    void processActive (const Input& in, int offset, MidiEventList& out);
    void startNote (float pitch, float levelDb, int offset, MidiEventList& out);
    void endNote (int offset, MidiEventList& out);
    void switchNote (int newNote, float levelDb, int offset, MidiEventList& out);
    void acceptPitch (float pitch, float levelDb, bool step, int offset, MidiEventList& out);
    void sendBend (float semitones, int offset, MidiEventList& out);
    void sendBendValue (int value14, int offset, MidiEventList& out);
    void sendController (int number, int value, int offset, MidiEventList& out);
    int  velocityFor (float levelDb) const noexcept;
    int  noteOnVelocity (float levelDb, bool softStart) const noexcept;
    float attackSlopeDbPerMs() const noexcept;
    int  clampNote (int n) const noexcept;
    float periodInHops (float pitch) const noexcept;
    bool  attackInProgress() const noexcept;
    void resetSmoother (float pitch) noexcept;
    float smooth (float pitch) noexcept;
    void fillHistory (float pitch) noexcept;
    void pushHistory (float pitch) noexcept;

    Settings cfg;
    bool haveCfg = false;
    double hopRate = 750.0;

    // timing, in hops
    int confirmHops = 3, holdHops = 22, quietHopsNeeded = 2, stableHops = 2, octaveStableHops = 8;
    int transitionMaxHops = 18, minRetriggerHops = 30, ccMinHops = 4;
    int doubtHops = 22, subharmonicGuardHops = 75, sameNoteHops = 150, deferMaxHops = 34;
    int attackWindowHops = 52, levelAgoHops = 11;
    // playing aids: holdHops above follows Settings::bridgeMs; the jump guard
    // ("wrong string filter") watches jumpWindowHops after a note began and
    // makes a wide leap last jumpLockHops before it is believed
    int jumpWindowHops = 60, jumpLockHops = 38;
    float onsetClarity = 0.80f, sustainClarity = 0.55f, holdPeriods = 1.15f, onsetPeriods = 1.0f;
    float peakDecayPerHop = 0.004f, recentDecayPerHop = 0.08f, onsetGateDecayPerHop = 0.027f;
    float exprAttack = 0.2f, exprRelease = 0.03f;
    float exprSmoothCoeff = 1.0f;   // extra one-pole on the controller level, 1 = none

    // note state
    bool  active = false;
    int   curNote = 60;
    int   curChannel = 0;
    float lastPitch = 60.0f;   // last accepted raw pitch
    float outPitch = 60.0f;    // pitch after the bend mode has been applied
    int   noteAgeHops = 0;
    int   hopsSinceAttack = 1000000;  // since the bow started this stretch of sound
    int   unvoicedHops = 0;
    int   quietHops = 0;
    float notePeakDb = -120.0f;
    float recentMaxDb = -120.0f;
    bool  dipArmed = false;
    float dipMinDb = 0.0f;

    // short level memory, to see whether the level is still rising
    static constexpr int kLevelHistory = 16;
    float levelHistory[kLevelHistory] = {};
    int   levelHistoryPos = 0;

    // longer level memory, only read at a note-on (articulation)
    static constexpr int kRiseHistory = 64;
    float riseHistory[kRiseHistory] = {};
    int   riseHistoryPos = 0;

    // onset
    int   candCount = 0;
    float candPitch = 0.0f;     // where the current run of consistent estimates began
    float lastLevelDb = -120.0f;
    float onsetGateDb = -200.0f; // raised right after a note, falls back with time
    float idleMinDb = -200.0f;   // lowest level since the last note ended
    int   hopsSinceNoteEnd = 1000000;
    float lastEndedPitch = -100.0f;
    int   deferHops = 0;         // quantized mode: waiting for an unclear start to settle

    // step / slide discrimination
    static constexpr int kHistory = 4;
    float history[kHistory] = { 0, 0, 0, 0 };
    int   historyPos = 0;
    bool  inTransition = false;
    float frozenPitch = 0.0f;
    float anchorPitch = 0.0f;    // where the last small jump started from
    int   anchorHops = 1000000;  // hops since then
    float transitionPrev = 0.0f;
    int   transitionHops = 0;
    int   stableCount = 0;
    bool  jumpWatch = false;     // this transition began inside the jump guard's window

    // vibrato filter (moving average with a dead band)
    std::vector<float> smoothBuffer;   // sized for the longest window
    int    smoothLen = 1;              // the part of it in use
    int    smoothPos = 0;
    double smoothSum = 0.0;

    // controller state
    int   lastBend14 = -1;
    int   lastExpression = -1;
    int   hopsSinceExpression = 1000;
    float expressionDb = -120.0f;
    float exprSmoothDb = -120.0f;  // expressionDb after the extra smoothing
    unsigned ownedControllers = 0; // kOwn* bits: sent by us, must be handed back
};

} // namespace vm
