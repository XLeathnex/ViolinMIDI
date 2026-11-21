// ViolinMIDI core - the last stage before the MIDI port.
//
// The note tracker decides what the violin plays. This stage decides what of
// it is sent: the key filter, the pentatonic sustain and the triad on a second
// channel. Real-time safe: fixed-size state, no allocation, no locks.
//
// Input: the tracker's events of one analysis hop, all on Settings::midiChannel:
// note-on (0x90, velocity > 0), note-off (0x80), pitch bend (0xE0), controller
// (0xB0). A legato change arrives as "bend, [CC11], note-on new, note-off old";
// a fresh note as "bend, [CC11], note-on".
//
// The rules (R-numbers are referred to in the tests):
//
// R0  With keyFilter and triad both off the output equals the input, byte for
//     byte and in the same order.
//
// R1  Key filter (keyFilter on). A note is "allowed" if its pitch class is in
//     scale::mask (keyRoot, scaleMode, pentatonic).
//     a) An allowed note-on is sent.
//     b) A note-on that is not allowed is not sent. The note sounding until
//        then ends at once (note-off at the same sample offset) - unless the
//        sustain is active, then it is kept ("held").
//     c) Sustain is active when keyFilter, pentatonic and pentaSustain are all
//        on. Then the tracker's note-off for the sounding note is also not
//        sent while levelDb >= gateDb: the note is held.
//     d) A held note ends: when an allowed note-on arrives (note-off for the
//        held note first, then the new note-on, same offset); at the first hop
//        with levelDb < gateDb; or when a setting changes (R5).
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
// R4  Triad (triad on). Together with every note-on sent on the main channel
//     the notes of scale::triad (note, keyRoot, scaleMode, triadWithRoot) start
//     on triadChannel, same sample offset, same velocity, right after the main
//     note-on. They end at the same sample offset as their main note: with its
//     note-off, or - on a legato change - before the notes of the next triad
//     start. Pitch bend and CC11 sent on the main channel are repeated on the
//     triad channel. If triadChannel equals midiChannel the root is not doubled,
//     nothing is repeated, and on a legato change the old main note ends before
//     the new one starts.
//
// R5  Changed settings, compared with the previous call:
//     a) keyFilter, keyRoot, scaleMode, pentatonic or pentaSustain: a held note
//        ends at once. A sounding note that the new settings do not allow ends
//        at once (with its triad); R3 applies to it from then on. A sounding
//        note that is still allowed carries on; its triad is exchanged if it
//        changed (old notes off, then new notes on).
//     b) triad, triadChannel or triadWithRoot: the triad notes end at once on
//        the channel they were started on, and bend and CC11 are returned to
//        neutral there (8192 and 127) if this stage sent any. If the triad is
//        now on and a main note is sounding, its triad starts at once.
//     c) midiChannel: the tracker ends its own note on the old channel; a held
//        note is ended here, on the channel it was started on.
//
// The master switch (Settings::enabled) is handled by the Engine: it shuts the
// tracker down and calls releaseAll().
#pragma once

#include "Types.h"

namespace vm
{

class OutputStage
{
public:
    /** Upper bound of events one process() call appends for `numIn` input events. */
    static constexpr int maxEventsFor (int numIn) noexcept { return 12 * numIn + 40; }

    void prepare (double hopsPerSecond) noexcept;

    /** Forgets everything without sending MIDI. */
    void reset() noexcept;

    /** Once per analysis hop. `in` are the tracker's events of this hop (may be
        empty), `levelDb` the input level of this hop. Appends to `out`. */
    void process (const MidiEvent* in, int numIn, float levelDb, const Settings& settings,
                  int sampleOffset, MidiEventList& out);

    /** Ends every note this stage keeps sounding (main, held, triad) and
        returns the bend on both channels and CC11 on the triad channel to
        neutral if this stage moved them. The key-filter state is forgotten. */
    void releaseAll (int sampleOffset, MidiEventList& out);

    bool isSounding() const noexcept  { return cur.note >= 0; }   ///< a main note is being sent
    bool isHeld() const noexcept      { return cur.note >= 0 && cur.held; }
    int  note() const noexcept        { return cur.note; }        ///< sounding main note or -1
    int  triadCount() const noexcept  { return triadCount_; }
    int  triadNote (int i) const noexcept { return triadNotes[i]; }
    /** Semitones between the tracker's note and the note sounding (R3), else 0. */
    float pitchOffset() const noexcept { return cur.note >= 0 && ! cur.held ? (float) (cur.src - cur.note) : 0.0f; }

private:
    struct Voice
    {
        int  note = -1;      ///< what sounds, -1 = nothing
        int  channel = 0;    ///< 0..15, the channel it was started on
        int  src = -1;       ///< the tracker note it stands for
        bool held = false;   ///< kept although the tracker has left it
    };

    void applySettings (const Settings& s, int offset, MidiEventList& out);
    void handle (const MidiEvent& e, float levelDb, int offset, MidiEventList& out);
    void startMain (int note, int velocity, int srcNote, int channel, int offset, MidiEventList& out);
    void endVoice (Voice& v, int offset, MidiEventList& out);
    void beginHold (int offset, MidiEventList& out);
    void startTriad (int offset, MidiEventList& out);
    void endTriad (int offset, MidiEventList& out);
    void neutraliseTriadChannel (int channel, int offset, MidiEventList& out);
    void sendBend (int value14, int offset, MidiEventList& out);
    void forwardBend (int offset, MidiEventList& out);
    bool allowed (int note) const noexcept;
    bool sustainActive() const noexcept { return cfg.keyFilter && cfg.pentatonic && cfg.pentaSustain; }
    /** A note left behind on the previous MIDI channel is never held (R5c). */
    bool canHold() const noexcept       { return sustainActive() && cur.channel == cfg.midiChannel - 1; }
    bool mirror() const noexcept        { return cfg.triad && cfg.triadChannel != cfg.midiChannel; }

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
    bool  blocked = false;   // the tracker's note was not sent (R1b)
    int   settleCount = 0;
    int   settleTarget = -1;

    // what has been sent
    int   sentBend14 = 8192;
    int   sentExpression = -1;     // last CC11 value passed on, -1 = none yet
    int   velocity = 100;          // of the sounding main note

    int   triadNotes[3] = { -1, -1, -1 };
    int   triadCount_ = 0;
    int   triadChan = 1;           // 0..15, the channel the triad notes were started on
    int   triadBend14 = 8192;
    int   triadExpression = -1;
    bool  triadBendMoved = false, triadExpressionMoved = false;
};

} // namespace vm
