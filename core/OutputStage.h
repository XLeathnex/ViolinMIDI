// ViolinMIDI core - the last stage before the MIDI port.
//
// The note tracker decides what the violin plays. This stage decides what of
// it is sent: the key filter, the pentatonic sustain, the harmony voices on a
// second channel, freeze, and MPE. Real-time safe: fixed-size state, no
// allocation, no locks.
//
// Input: the tracker's events of one analysis hop, all on Settings::midiChannel:
// note-on (0x90, velocity > 0), note-off (0x80), pitch bend (0xE0), controller
// (0xB0). A legato change arrives as "bend, [controller], note-on new, note-off
// old"; a fresh note as "bend, [controller], note-on". The controllers 1, 2 and
// 11 carry the bow loudness ("dynamics controllers").
//
// The rules (R-numbers are referred to in the tests):
//
// R0  With keyFilter, triad, mpe and both freeze switches off the output
//     equals the input, byte for byte and in the same order.
//
// R1  Key filter (keyFilter on). A note is "allowed" if its pitch class is in
//     scale::mask (keyRoot, scaleMode, pentatonic).
//     a) An allowed note-on is sent.
//     b) A note-on that is not allowed is not sent. The note sounding until
//        then ends at once (note-off at the same sample offset) - unless the
//        sustain is active, then it is kept ("held").
//     c) Sustain is active when keyFilter and pentaSustain are on and the
//        allowed notes are fewer than seven (pentatonic on, or a pentatonic or
//        blues scale). Then the tracker's note-off for the sounding note is
//        also not sent while levelDb >= gateOffDb: the note is held.
//     d) A held note ends: when an allowed note-on arrives (note-off for the
//        held note first, then the new note-on, same offset); at the first hop
//        with levelDb < gateOffDb; or when a setting changes (R5).
//     e) Never a note-off for a note that is not sounding, never a note-on for
//        a note that is already sounding on that channel.
//
// R2  Pitch bend and controllers follow the tracker, except that a held note
//     is not bent: when a hold begins the bend returns to the centre (8192, if
//     it is not there already) and the tracker's bends are kept back until the
//     hold ends. The note-on that ends a hold is preceded by the tracker's
//     current bend if that differs from the last bend sent.
//
// R3  Sliding into the key. While the tracker has a note that was not sent
//     (R1b), its pitch is watched: tracker note + its bend in semitones. Once
//     that pitch has stayed within 0.30 semitones of an allowed note for
//     30 ms, that note starts, with the velocity of the note-on that was not
//     sent and bent to the exact pitch. From then on the tracker's bends are
//     re-centred on the note actually sounding.
//
// R4  Harmony (triad on). The voices are scale::voices (root, keyRoot,
//     scaleMode, voicingMask (settings), harmonyMode == Diatonic) on triadChannel,
//     with the velocity of the main note. They always include the played note
//     where the voicing has it (1 - 3 - 5 sends all three).
//     a) The root is the semitone nearest to the pitch that sounds (tracker
//        note + its bend), not the note number: a note bent a whole tone up
//        is harmonised as the note it has become. This is what keeps a chord
//        from being bent into a foreign one.
//     b) Start. With every main note-on, at once and right after it, on the
//        semitone nearest to the pitch (the tracker's note, unless its bend
//        is beyond half a semitone).
//     c) While the main note sounds, the root is kept as long as the pitch
//        stays within 0.65 semitones of it. A pitch nearer to another
//        semitone for 30 ms exchanges the voices: old notes off, the bend
//        for the new root, new notes on, same sample offset. When a hold
//        begins (R1b/c) the main note returns to its own pitch at once, and
//        the voices are exchanged in that same hop if the root had moved.
//     d) The voices end at the same sample offset as their main note: with
//        its note-off, or - on a legato change - before the next ones start.
//     e) Pitch bend on the harmony channel is the distance of the pitch from
//        the root (the same value as on the main channel while the root is
//        the main note). Dynamics controllers sent on the main channel are
//        repeated on the harmony channel.
//     f) If triadChannel equals midiChannel (and mpe is off) the harmony is
//        simpler: the root is the main note, the main note is not doubled,
//        nothing is repeated, and on a legato change the old main note ends
//        before the new one starts.
//
// R5  Changed settings, compared with the previous call:
//     a) keyFilter, keyRoot, scaleMode, pentatonic or pentaSustain: a held note
//        ends at once. A sounding note that the new settings do not allow ends
//        at once (with its harmony); R3 applies to it from then on. A sounding
//        note that is still allowed carries on; its harmony is exchanged if it
//        changed (old notes off, then new notes on).
//     b) triad, triadChannel, voicing, harmonyMode or the voices of the voicing
//        (voicingMask: freeVoicing or voicingVoices): if the
//        harmony stays on its channel and the new settings ask for exactly
//        the voices that sound, nothing is sent. Otherwise the harmony notes
//        end at once on the channel they were started on, and bend and
//        dynamics controllers are returned to neutral there (8192; 127, for
//        controller 1: 0) if this stage sent any and the channel is left. If
//        the harmony is now on and a main note is sounding, it starts again
//        (R4b). With mpe, a main note on the member channel the harmony has
//        just taken moves to a free channel at once (off, then on).
//     c) midiChannel: the tracker ends its own note on the old channel; a held
//        note is ended here, on the channel it was started on.
//     d) mpe: everything on the solo track ends on the channels it was started
//        on, bends there return to the centre, and the note the tracker still
//        has starts again by R3.
//
// R6  MPE (mpe on). The main voice does not use midiChannel: every note-on
//     takes the next of the member channels 2..16 in turn, skipping channels
//     that are still in use (the previous note until its note-off, a frozen
//     note, the harmony channel while the harmony is on). Bend, dynamics
//     controllers and - with the same value - channel pressure (0xD0) go to
//     the channel of the sounding note; those that arrive in the same hop
//     before a note-on belong to the new note and are sent on its channel
//     before its note-on. On a legato change the old note therefore keeps its
//     pitch until its note-off. Other controllers go to channel 1. When mpe is
//     switched on (also at the first call) the zone is announced on channel 1:
//     controller 101 = 0, 100 = 6, 6 = 15; switched off: 6 = 0. The Engine
//     sets the bend range to 48 semitones while mpe is on.
//
// R7  Freeze, separately for the solo track (the main voice) and the harmony
//     track.
//     a) Switched on while something sounds on that track: those notes are
//        frozen. Switched on while nothing sounds: the next note (or harmony)
//        that starts is frozen, right after its note-on.
//     b) From then on nothing is sent on that track - no note-on, note-off,
//        bend, controller - while this stage goes on working as if it were,
//        so the other track follows the playing as before.
//     c) Switched off: the frozen notes end (note-off on the channel they
//        were started on). If the player is in the middle of a note, that
//        track is brought up to date first (bend, controllers) and the note
//        starts - unless it is the very note that was frozen, which then
//        simply carries on.
//     d) releaseAll() ends frozen notes too; a switch that is still on then
//        waits for the next note as in (a).
//     e) Where the two tracks meet on one channel (R4f, or a harmony channel
//        inside the MPE zone) a frozen note stands for the same note of the
//        other track: that track's note-on and note-off for it are not sent
//        while it is frozen, and when the freeze ends the note stays if the
//        other track still has it. The harmony sends no bend and no
//        controllers on a channel that holds a frozen solo note (they are
//        made up when the freeze ends). With R4f the channel's bend and
//        controllers are the solo track's and reach frozen harmony notes too.
//
// R8  Ringing on (overlapMs, releaseMs; both 0: nothing here applies).
//     a) Overlap. At a change of note - the tracker's legato change, or an
//        exchange of the harmony voices (R4c) - what sounded before is not
//        ended but rings on for overlapMs, then gets its note-offs. With mpe
//        the old main note has a channel of its own and is faded there (c);
//        on a channel it shares with the new notes it rings at full level.
//     b) Release. When the tracker ends the main note with nothing following
//        (the level fell below the out threshold), the note and its harmony
//        ring on for releaseMs and are faded out over that time.
//     c) A fade lowers controller 11 on that channel step by step from the
//        value it had to 0 (with mpe the channel pressure as well); after the note-offs the controller gets back the
//        value it would have had. A new note starting on a channel where
//        something is fading ends the fade (the controller is the new note's
//        now) and what rings there has overlapMs left at most.
//     d) A note-on for a note that is still ringing on that channel takes it
//        over: note-off first, then the note-on (R1e holds for ringing notes
//        too).
//     e) While something rings on a channel and its track has no live note
//        there, pitch bend and controller 11 for that channel are held back;
//        the channel is brought up to date when the ringing ends or a new
//        note starts.
//     f) Nothing rings on from a frozen track (R7), from a held note (R1c), or
//        across a change of key, harmony or channel settings (R5) and
//        releaseAll(): there everything ringing ends at once.
//
// R9  External chords (2.1, harmonySource == ExternalChords). Before every
//     process() call the Engine hands over a chord::Context (setChord): the
//     chord tones, the chord scale (seven notes above a root), its pentatonic,
//     and whether keys are being struck right now. Until the first chord it
//     describes the start key (keyRoot, scaleMode) without chord tones.
//     a) Filter. The mask of R1 is the chord scale with keyFilter, the chord's
//        pentatonic with pentaFilter.
//     b) Voices. chord::voices (root, context, voicingMask, chordMode, targets,
//        ! mirror()) instead of scale::voices. `targets` (Hybrid A and B) is
//        decided whenever the harmony starts (R4b, also after R4c): the root's
//        pitch class is a chord tone. Hybrid B: a root that is not a chord tone
//        and has stayed for hybridLockMs (R4c's 0.65-semitone window) switches
//        to chord tones once, by (d).
//     c) A new chord (the context's serial changed):
//        - a held note (R1c) the new mask does not allow ends; R3 applies to the
//          tracker's note from then on (its 30 ms count starts again);
//        - a note the player is bowing carries on, allowed or not: the filter
//          decides at note starts only;
//        - what rings on the harmony channel (R8) ends at once, if the
//          harmony has a channel of its own (on a shared one, R4f, it rings out);
//        - the harmony is re-voiced by (d), `targets` decided again. A frozen
//          harmony (R7) stays; its live state follows as in R7b.
//     d) Re-voicing changes only what differs: note-offs for the voices that
//        go, then note-ons for the new ones (velocity of the harmony), same
//        sample offset; voices in both carry on untouched. No overlap (R8a),
//        no bend or controller is resent. revoiceCount() counts them.
//     e) A harmony that would start while keys are being struck waits for that
//        chord, at most 35 ms (ChordTracker::kMaxGatherSec); the main note is
//        never delayed. If the note ends meanwhile, nothing starts.
//     f) With harmonySource == FixedKey the context is ignored and everything
//        is as in 2.0, apart from the two filter changes of 2.1: pentaFilter
//        ("Nur Pentatonik-Toene", the pentatonic of the key) and pentaSustain
//        ("Ton halten") for every filter (R1, R1c).
//
// Settings::harmonyOffset (-0.5 .. 0) makes the harmony softer: the velocity
// of the voices and the dynamics controllers repeated on the harmony channel
// are multiplied by (1 + harmonyOffset). Neutral values are not scaled.
//
// The master switch (Settings::enabled) is handled by the Engine: it shuts the
// tracker down and calls releaseAll().
#pragma once

#include "Chord.h"
#include "Scale.h"
#include "Types.h"

namespace vm
{

class OutputStage
{
public:
    /** Upper bound of events one process() call appends for `numIn` input events. */
    static constexpr int maxEventsFor (int numIn) noexcept { return 40 * numIn + 260; }

    void prepare (double hopsPerSecond) noexcept;

    /** Forgets everything without sending MIDI. */
    void reset() noexcept;

    /** Once per analysis hop. `in` are the tracker's events of this hop (may be
        empty), `levelDb` the input level of this hop. Appends to `out`. */
    void process (const MidiEvent* in, int numIn, float levelDb, const Settings& settings,
                  int sampleOffset, MidiEventList& out);

    /** Ends every note this stage keeps sounding (main, held, harmony, frozen)
        and returns bends and the harmony channel's controllers to neutral if
        this stage moved them. The key-filter state is forgotten. */
    void releaseAll (int sampleOffset, MidiEventList& out);

    bool isSounding() const noexcept  { return cur.note >= 0; }   ///< the player has a main note (sent, unless the solo track is frozen)
    bool isHeld() const noexcept      { return cur.note >= 0 && cur.held; }
    int  note() const noexcept        { return cur.note; }        ///< that main note or -1
    /** The notes sounding on the harmony channel (the frozen ones while frozen). */
    int  triadCount() const noexcept  { return harmFreeze == Freeze::On ? harmDroneCount : triadCount_; }
    int  triadNote (int i) const noexcept { return harmFreeze == Freeze::On ? harmDrone[i] : triadNotes[i]; }
    /** Semitones between the tracker's note and the note sounding (R3), else 0. */
    float pitchOffset() const noexcept { return cur.note >= 0 && ! cur.held ? (float) (cur.src - cur.note) : 0.0f; }
    /** The semitone the harmony is built on right now, or -1 if no harmony is sounding live. */
    int  harmonyRoot() const noexcept { return harmActive && harmFreeze != Freeze::On ? harmRoot : -1; }
    /** The harmony track is frozen and has notes. */
    bool harmonyFrozen() const noexcept { return harmFreeze == Freeze::On && harmDroneCount > 0; }
    /** The semitone the frozen harmony was built on, or -1 if the harmony is not frozen. */
    int  frozenHarmonyRoot() const noexcept { return harmonyFrozen() ? harmDroneRoot : -1; }
    /** The frozen solo note or -1. */
    int  frozenNote() const noexcept  { return soloFreeze == Freeze::On ? soloDrone.note : -1; }

    /** R9 (2.1): the chord context for the next process() call. Same thread as process(). */
    void setChord (const chord::Context& c) noexcept   { pendingChord = c; }
    /** Counts the re-voicings of R9d (for the audio harmonizer, Engine::updateHarmonizerFrame). */
    uint32_t revoiceCount() const noexcept             { return revoices; }

private:
    enum class Track  { Solo, Harmony };
    enum class Freeze { Off, Armed, On };   // Armed: switched on, waiting for something to freeze

    struct Voice
    {
        int  note = -1;      ///< what sounds, -1 = nothing
        int  channel = 0;    ///< 0..15, the channel it was started on
        int  src = -1;       ///< the tracker note it stands for
        bool held = false;   ///< kept although the tracker has left it
    };

    static constexpr int kDynCount = 3;   // controllers 1, 2 and 11

    void applySettings (const Settings& s, int offset, MidiEventList& out);
    void applyFreeze (const Settings& n, int offset, MidiEventList& out);
    void handle (const MidiEvent& e, bool noteOnAhead, float levelDb, int offset, MidiEventList& out);
    void startMain (int note, int velocity, int srcNote, int channel, int offset, MidiEventList& out);
    void endVoice (Voice& v, int offset, MidiEventList& out, int tailHops = 0, bool fade = false);
    void beginHold (int offset, MidiEventList& out);

    void beginHarmony (int offset, MidiEventList& out);
    void updateHarmony (int offset, MidiEventList& out);
    void startTriad (int offset, MidiEventList& out);
    void endTriad (int offset, MidiEventList& out, int tailHops = 0, bool fade = false);
    void neutraliseTriadChannel (int channel, int offset, MidiEventList& out);
    void handBackChannel (int channel, int offset, MidiEventList& out);
    void mirrorBend (int offset, MidiEventList& out);
    void mirrorDynamics (int index, int value, int offset, MidiEventList& out);
    float harmonyPitch() const noexcept;
    int  harmonyBend() const noexcept;

    void sendBend (int value14, int offset, MidiEventList& out);
    void forwardBend (int offset, MidiEventList& out);
    void syncSolo (int channel, int offset, MidiEventList& out);

    void captureSolo (int offset, MidiEventList& out);
    void releaseSolo (int offset, MidiEventList& out);
    void captureHarmony (int offset, MidiEventList& out);
    void releaseHarmony (int offset, MidiEventList& out);

    // R8
    struct Tail
    {
        bool active = false;
        int  channel = 0;
        int  notes[scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        int  count = 0;
        int  hops = 0, total = 1;     ///< how long it has rung and how long it will
        bool fade = false;
        int  startValue = 127, lastValue = 127;   ///< controller 11 at the start of the fade and as last sent
        int  startPressure = -1, lastPressure = -1; ///< mpe: channel pressure likewise, -1 = not faded
    };

    void startTail (int channel, const int* notes, int count, int hops, bool fade, int offset, MidiEventList& out);
    void finishTail (Tail&, int offset, MidiEventList& out);
    void advanceTails (int offset, MidiEventList& out);
    void stopFades (int channel, int offset, MidiEventList& out);
    void takeFromTails (int channel, int note, int offset, MidiEventList& out);
    void flushTails (int offset, MidiEventList& out);
    void flushTailsOn (int channel, int offset, MidiEventList& out);
    bool tailOn (int channel) const noexcept;
    bool heldBack (Track, int status, int channel, int data1) const noexcept;
    int  liveExpression (int channel) const noexcept;
    int  overlapHops() const noexcept;
    int  releaseHops() const noexcept;
    int  forHarmony (int value) const noexcept;
    int  harmonyVelocity() const noexcept;

    void sendMpeZone (int memberChannels, int offset, MidiEventList& out);
    int  allocChannel() noexcept;
    bool channelBusy (int channel) const noexcept;

    /** Everything goes out through these two: emit() drops what belongs to a frozen track. */
    void emit (Track, int offset, int status, int channel, int data1, int data2, MidiEventList& out);
    void emitAlways (int offset, int status, int channel, int data1, int data2, MidiEventList& out);
    void append (const MidiEvent&, MidiEventList& out);

    bool allowed (int note) const noexcept;
    bool soloHas (int channel, int note) const noexcept;      ///< that track sounds this note there (frozen or not)
    bool harmonyHas (int channel, int note) const noexcept;
    bool frozenHas (int channel, int note) const noexcept;
    bool sustainActive() const noexcept
    {
        return cfg.keyFilter && cfg.pentaSustain && (cfg.pentatonic || scale::noteCount (cfg.scaleMode) < 7);
    }
    /** A note left behind on the previous MIDI channel is never held (R5c). */
    bool canHold() const noexcept       { return sustainActive() && (cfg.mpe || cur.channel == cfg.midiChannel - 1); }
    /** The harmony has a channel of its own (R4a-e), else R4f. */
    bool mirror() const noexcept        { return cfg.triad && (cfg.mpe || cfg.triadChannel != cfg.midiChannel); }

    Settings cfg;
    bool haveCfg = false;
    unsigned scaleMask = 0xFFFu;
    int settleHops = 22;

    Voice cur, old;          // `old` only between a legato note-on and the note-off that follows it

    // the tracker's side
    bool  srcActive = false;
    int   srcNote = -1;
    int   srcVelocity = 100;
    int   srcBend14 = 8192;  // last bend the tracker sent
    int   srcDyn[kDynCount] = { -1, -1, -1 };   // last value of each dynamics controller, -1 = none yet
    int   srcPressure = -1;  // MPE: the loudness the next note starts with, -1 = none
    bool  blocked = false;   // the tracker's note was not sent (R1b)
    int   settleCount = 0;
    int   settleTarget = -1;

    // what this stage has sent on the solo track - or would have, while that track is frozen
    int   sentBend14 = 8192;
    int   sentDyn[kDynCount] = { -1, -1, -1 };
    int   sentPressure = -1;
    int   velocity = 100;          // of the sounding main note
    int   lastMainChannel = 0;     // 0..15

    // harmony, in the same sense
    int   triadNotes[scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int   triadCount_ = 0;
    int   triadChan = 1;           // 0..15, the channel the harmony notes were started on
    int   triadBend14 = 8192;
    int   triadDyn[kDynCount] = { -1, -1, -1 };
    bool  triadBendMoved = false;
    bool  triadDynMoved[kDynCount] = { false, false, false };
    bool  harmActive = false;      // the harmony of the sounding main note has started
    int   harmRoot = -1;
    int   harmCand = -1;           // another root the pitch has moved to (R4c)
    int   harmCount = 0;

    // freeze
    Freeze soloFreeze = Freeze::Off, harmFreeze = Freeze::Off;
    Voice  soloDrone;
    int    harmDrone[scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int    harmDroneCount = 0;
    int    harmDroneChan = 0;
    int    harmDroneRoot = -1;

    // what really went out, per channel
    int   chanBend[16];
    int   chanDyn[16][kDynCount];
    int   chanPressure[16];

    int   mpeNext = 1;             // the member channel the next note tries first, 1..15

    static constexpr int kMaxTails = 8;
    Tail  tails[kMaxTails];
    double hopRate = 750.0;

    // external chords (R9)
    chord::Context pendingChord;   // handed over by setChord() for the next process()
    uint32_t revoices = 0;
};

} // namespace vm
