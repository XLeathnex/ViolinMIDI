#include "OutputStage.h"

#include <algorithm>
#include <cmath>

namespace vm
{

namespace
{
    constexpr int    kCentre = 8192;
    constexpr double kSettleSec = 0.030;
    constexpr float  kSettleTolerance = 0.30f;
    constexpr float  kHarmonyClear = 0.25f;       // R4b: this close to a semitone the pitch is clear
    constexpr float  kHarmonyHysteresis = 0.15f;  // R4c: semitones past the half-way point

    // The controllers that may carry the bow loudness, and where they rest.
    constexpr int kDynNumber[3]  = { 1, 2, 11 };
    constexpr int kDynNeutral[3] = { 0, 127, 127 };

    inline int dynIndex (int controller) noexcept
    {
        return controller == 11 ? 2 : (controller == 1 ? 0 : (controller == 2 ? 1 : -1));
    }

    inline int   clampi (int v, int lo, int hi) noexcept       { return std::min (hi, std::max (lo, v)); }
    inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }

    inline bool isNoteOn (const MidiEvent& e) noexcept
    {
        return (e.bytes[0] & 0xF0) == 0x90 && (e.bytes[2] & 0x7F) > 0;
    }
}

void OutputStage::prepare (double hopsPerSecond) noexcept
{
    settleHops = std::max (1, (int) std::lround (kSettleSec * std::max (50.0, hopsPerSecond)));
    reset();
}

void OutputStage::reset() noexcept
{
    cur = Voice();
    old = Voice();
    haveCfg = false;
    scaleMask = 0xFFFu;
    srcActive = false;
    srcNote = -1;
    srcVelocity = 100;
    srcBend14 = kCentre;
    srcPressure = -1;
    blocked = false;
    settleCount = 0;
    settleTarget = -1;
    sentBend14 = kCentre;
    sentPressure = -1;
    velocity = 100;
    lastMainChannel = 0;
    triadCount_ = 0;
    triadBend14 = kCentre;
    triadBendMoved = false;
    harmActive = harmPending = false;
    harmRoot = -1;
    harmWait = 0;
    harmCand = -1;
    harmCount = 0;
    soloFreeze = harmFreeze = Freeze::Off;
    soloDrone = Voice();
    harmDroneCount = 0;
    harmDroneChan = 0;
    mpeNext = 1;

    for (int k = 0; k < kDynCount; ++k)
    {
        srcDyn[k] = sentDyn[k] = triadDyn[k] = -1;
        triadDynMoved[k] = false;
    }

    for (int ch = 0; ch < 16; ++ch)
    {
        chanBend[ch] = kCentre;
        chanPressure[ch] = -1;
        for (int k = 0; k < kDynCount; ++k)
            chanDyn[ch][k] = -1;
    }
}

bool OutputStage::allowed (int note) const noexcept
{
    return ! cfg.keyFilter || scale::contains (scaleMask, note);
}

//==============================================================================
void OutputStage::emitAlways (int offset, int status, int channel, int data1, int data2, MidiEventList& out)
{
    channel &= 0x0F;
    data1 &= 0x7F;
    data2 &= 0x7F;

    MidiEvent e;
    e.sampleOffset = offset;
    e.size = (uint8_t) (status == 0xD0 ? 2 : 3);
    e.bytes[0] = (uint8_t) (status | channel);
    e.bytes[1] = (uint8_t) data1;
    e.bytes[2] = (uint8_t) data2;
    append (e, out);
}

void OutputStage::append (const MidiEvent& e, MidiEventList& out)
{
    out.push_back (e);

    const int status = e.bytes[0] & 0xF0, channel = e.bytes[0] & 0x0F;
    const int data1 = e.bytes[1] & 0x7F, data2 = e.bytes[2] & 0x7F;

    if (status == 0xE0)
    {
        chanBend[channel] = data1 | (data2 << 7);
    }
    else if (status == 0xD0)
    {
        chanPressure[channel] = data1;
    }
    else if (status == 0xB0)
    {
        const int k = dynIndex (data1);
        if (k >= 0)
            chanDyn[channel][k] = data2;
    }
}

void OutputStage::emit (Track track, int offset, int status, int channel, int data1, int data2, MidiEventList& out)
{
    if ((track == Track::Solo ? soloFreeze : harmFreeze) == Freeze::On)   // R7b
        return;

    if ((status == 0x90 || status == 0x80) && frozenHas (channel & 0x0F, data1))   // R7e
        return;

    // R7e: the harmony does not bend a frozen solo note whose channel it has moved onto.
    if (track == Track::Harmony && status != 0x90 && status != 0x80
        && soloFreeze == Freeze::On && soloDrone.channel == (channel & 0x0F))
        return;

    emitAlways (offset, status, channel, data1, data2, out);
}

bool OutputStage::soloHas (int channel, int note) const noexcept
{
    if (soloFreeze == Freeze::On)
        return soloDrone.channel == channel && soloDrone.note == note;

    return (cur.note == note && cur.channel == channel) || (old.note == note && old.channel == channel);
}

bool OutputStage::harmonyHas (int channel, int note) const noexcept
{
    const bool frozen = harmFreeze == Freeze::On;
    if ((frozen ? harmDroneChan : triadChan) != channel)
        return false;

    const int count = frozen ? harmDroneCount : triadCount_;
    const int* notes = frozen ? harmDrone : triadNotes;
    for (int i = 0; i < count; ++i)
        if (notes[i] == note)
            return true;
    return false;
}

bool OutputStage::frozenHas (int channel, int note) const noexcept
{
    return (soloFreeze == Freeze::On && soloHas (channel, note))
        || (harmFreeze == Freeze::On && harmonyHas (channel, note));
}

//==============================================================================
void OutputStage::process (const MidiEvent* in, int numIn, float levelDb, const Settings& settings,
                           int offset, MidiEventList& out)
{
    applySettings (settings, offset, out);

    for (int i = 0; i < numIn; ++i)
    {
        // R6: what comes before a note-on in the same hop belongs to the new note.
        bool noteOnAhead = false;
        if (cfg.mpe)
            for (int j = i + 1; j < numIn && ! noteOnAhead; ++j)
                noteOnAhead = isNoteOn (in[j]);

        handle (in[i], noteOnAhead, levelDb, offset, out);
    }

    // R1d: a held note lives on the bow. Below the threshold it ends.
    if (cur.note >= 0 && cur.held && levelDb < cfg.gateOffDb)
        endVoice (cur, offset, out);

    // R3: the tracker's note was not sent, but the finger may slide into the key.
    bool settling = false;
    if (blocked && srcActive)
    {
        const float pitch = (float) srcNote + (float) (srcBend14 - kCentre) / 8192.0f * cfg.bendRangeSemis;
        const int target = (int) std::lround (pitch);

        if (target >= 0 && target <= 127 && allowed (target) && std::abs (pitch - (float) target) <= kSettleTolerance)
        {
            settling = true;
            if (target != settleTarget)
            {
                settleTarget = target;
                settleCount = 0;
            }

            if (++settleCount >= settleHops)
            {
                if (cur.note >= 0)
                    endVoice (cur, offset, out);   // a held note makes way first
                startMain (target, srcVelocity, srcNote, cfg.mpe ? allocChannel() : cfg.midiChannel - 1, offset, out);
                blocked = false;
                settling = false;
            }
        }
    }

    if (! settling)
    {
        settleCount = 0;
        settleTarget = -1;
    }

    updateHarmony (offset, out);
}

void OutputStage::releaseAll (int offset, MidiEventList& out)
{
    // R7d. What is not frozen ends first: its note-offs go out, while everything
    // that was only kept back (it never sounded) and every note a frozen one
    // stood for (R7e) is dropped by emit(). Then the frozen notes end.
    const bool soloWasFrozen = soloFreeze == Freeze::On;
    const bool harmWasFrozen = harmFreeze == Freeze::On;
    const int  droneChannel = harmDroneChan;

    endVoice (old, offset, out);
    endVoice (cur, offset, out);
    endTriad (offset, out);

    const Voice drone = soloDrone;
    soloDrone = Voice();
    if (soloWasFrozen)
        emitAlways (offset, 0x80, drone.channel, drone.note, 0, out);

    if (harmWasFrozen)
        for (int i = 0; i < harmDroneCount; ++i)
            if (! (soloWasFrozen && droneChannel == drone.channel && harmDrone[i] == drone.note))
                emitAlways (offset, 0x80, droneChannel, harmDrone[i], 0, out);
    harmDroneCount = 0;

    soloFreeze = haveCfg && cfg.freezeSolo ? Freeze::Armed : Freeze::Off;
    harmFreeze = haveCfg && cfg.freezeHarmony ? Freeze::Armed : Freeze::Off;

    if (haveCfg)
    {
        const int harmonyChannel = cfg.triadChannel - 1;

        if (harmWasFrozen)
        {
            handBackChannel (droneChannel, offset, out);
            if (harmonyChannel != droneChannel && mirror())
                handBackChannel (harmonyChannel, offset, out);

            triadBend14 = kCentre;
            triadBendMoved = false;
            for (int k = 0; k < kDynCount; ++k)
            {
                triadDyn[k] = -1;
                triadDynMoved[k] = false;
            }
        }
        else
        {
            neutraliseTriadChannel (harmonyChannel, offset, out);
        }

        // Every bend this stage moved goes back to the centre - also one left behind on a
        // channel the solo track had while it was frozen (the centring there was held back).
        for (int ch = 0; ch < 16; ++ch)
            if (chanBend[ch] != kCentre)
                emitAlways (offset, 0xE0, ch, 0, 64, out);
    }

    srcActive = false;
    srcNote = -1;
    srcBend14 = kCentre;
    srcPressure = -1;
    sentBend14 = kCentre;
    sentPressure = -1;
    for (int k = 0; k < kDynCount; ++k)
        srcDyn[k] = sentDyn[k] = -1;
    blocked = false;
    settleCount = 0;
    settleTarget = -1;
}

//==============================================================================
void OutputStage::applySettings (const Settings& s, int offset, MidiEventList& out)
{
    Settings n = s;
    n.midiChannel    = clampi (n.midiChannel, 1, 16);
    n.triadChannel   = clampi (n.triadChannel, 1, 16);
    n.keyRoot        = scale::pitchClass (n.keyRoot);
    n.scaleMode      = scale::clampMode (n.scaleMode);
    n.voicing        = scale::clampVoicing (n.voicing);
    n.freeVoicing    = scale::cleanFreeMask (n.freeVoicing);
    n.harmonyMode    = n.harmonyMode == HarmonyMode::Chromatic ? HarmonyMode::Chromatic : HarmonyMode::Diatonic;
    n.bendRangeSemis = clampf (n.bendRangeSemis, 0.1f, 96.0f);
    n.gateOffDb      = std::min (n.gateOffDb, n.gateDb);

    if (! haveCfg)
    {
        cfg = n;
        haveCfg = true;
        scaleMask = scale::mask (cfg.keyRoot, cfg.scaleMode, cfg.pentatonic);
        soloFreeze = cfg.freezeSolo ? Freeze::Armed : Freeze::Off;
        harmFreeze = cfg.freezeHarmony ? Freeze::Armed : Freeze::Off;
        if (cfg.mpe)
            sendMpeZone (15, offset, out);
        return;
    }

    applyFreeze (n, offset, out);

    const bool keyChanged = n.keyFilter != cfg.keyFilter || n.keyRoot != cfg.keyRoot || n.scaleMode != cfg.scaleMode
                         || n.pentatonic != cfg.pentatonic || n.pentaSustain != cfg.pentaSustain;
    const bool triadChanged = n.triad != cfg.triad || n.triadChannel != cfg.triadChannel || n.voicing != cfg.voicing
                           || n.harmonyMode != cfg.harmonyMode || n.freeVoicing != cfg.freeVoicing;
    const bool mpeChanged = n.mpe != cfg.mpe;
    const bool channelChanged = mpeChanged || (! n.mpe && n.midiChannel != cfg.midiChannel);

    if (! keyChanged && ! triadChanged && ! channelChanged)
    {
        cfg = n;
        return;
    }

    if (mpeChanged)
    {
        // R5d: the solo track changes its channels. Everything there ends where it was started.
        endVoice (old, offset, out);
        endVoice (cur, offset, out);
        blocked = srcActive;

        const int harmonyChannel = cfg.triadChannel - 1;
        for (int ch = 0; ch < 16; ++ch)
        {
            const bool solo = cfg.mpe ? ch >= 1 : ch == cfg.midiChannel - 1;
            if (solo && chanBend[ch] != kCentre && ! (cfg.triad && ch == harmonyChannel))
                emit (Track::Solo, offset, 0xE0, ch, 0, 64, out);
        }

        sendMpeZone (n.mpe ? 15 : 0, offset, out);
        sentBend14 = n.mpe ? kCentre : chanBend[n.midiChannel - 1];
        sentPressure = -1;
    }

    // A harmony setting changed, but the voices that sound are the ones wanted: they carry on.
    bool keepHarmony = false;
    if (triadChanged && ! channelChanged && harmActive && cur.note >= 0 && n.triad && n.triadChannel == cfg.triadChannel)
    {
        const scale::Voices wanted = scale::voices (harmRoot, n.keyRoot, n.scaleMode, n.voicing,
                                                    n.harmonyMode == HarmonyMode::Diatonic, n.freeVoicing, ! mirror());
        keepHarmony = wanted.count == triadCount_ && triadChan == n.triadChannel - 1;
        for (int i = 0; keepHarmony && i < wanted.count; ++i)
            keepHarmony = wanted.notes[i] == triadNotes[i];
    }

    // What was started under the old settings ends on the old channels.
    if ((triadChanged || channelChanged) && ! keepHarmony)
    {
        endTriad (offset, out);

        // The second channel gets its controllers back when it is left.
        const bool mirrorsAfter = n.triad && (n.mpe || n.triadChannel != n.midiChannel);
        if (! mirrorsAfter || n.triadChannel != cfg.triadChannel)
            neutraliseTriadChannel (cfg.triadChannel - 1, offset, out);
    }

    if ((keyChanged || channelChanged) && cur.note >= 0 && cur.held)
        endVoice (cur, offset, out);

    cfg = n;
    scaleMask = scale::mask (cfg.keyRoot, cfg.scaleMode, cfg.pentatonic);

    if (keyChanged)
    {
        settleCount = 0;
        settleTarget = -1;

        if (cur.note >= 0 && ! allowed (cur.note))
        {
            endVoice (cur, offset, out);
            blocked = srcActive;
        }
    }

    // R6/R1e: the harmony has taken the member channel of the main note. That note
    // moves to a free channel at once (a held one ends, R3 applies from then on).
    if (triadChanged && cfg.mpe && cfg.triad && cur.note >= 0 && cur.channel == cfg.triadChannel - 1)
    {
        const Voice moved = cur;
        endVoice (cur, offset, out);
        if (moved.held)
            blocked = srcActive;
        else
            startMain (moved.note, velocity, moved.src, allocChannel(), offset, out);
    }

    // On a channel change the tracker ends its note in this very hop: no new harmony for it.
    if (cur.note >= 0 && ! channelChanged)
    {
        if (! cfg.triad)
        {
            endTriad (offset, out);
        }
        else if (harmActive)
        {
            const scale::Voices wanted = scale::voices (harmRoot, cfg.keyRoot, cfg.scaleMode, cfg.voicing,
                                                        cfg.harmonyMode == HarmonyMode::Diatonic, cfg.freeVoicing, ! mirror());
            bool same = wanted.count == triadCount_ && triadChan == cfg.triadChannel - 1;
            for (int i = 0; same && i < wanted.count; ++i)
                same = wanted.notes[i] == triadNotes[i];

            if (! same)
            {
                endTriad (offset, out);
                startTriad (offset, out);
            }
        }
        else if (! harmPending)
        {
            beginHarmony (offset, out);
        }
    }
}

void OutputStage::applyFreeze (const Settings& n, int offset, MidiEventList& out)
{
    if (n.freezeSolo != cfg.freezeSolo)
    {
        if (! n.freezeSolo)
            releaseSolo (offset, out);
        else if (cur.note >= 0)
            captureSolo (offset, out);
        else
            soloFreeze = Freeze::Armed;
    }

    if (n.freezeHarmony != cfg.freezeHarmony)
    {
        if (! n.freezeHarmony)
            releaseHarmony (offset, out);
        else if (triadCount_ > 0)
            captureHarmony();
        else
            harmFreeze = Freeze::Armed;
    }

    cfg.freezeSolo = n.freezeSolo;
    cfg.freezeHarmony = n.freezeHarmony;
}

//==============================================================================
void OutputStage::handle (const MidiEvent& e, bool noteOnAhead, float levelDb, int offset, MidiEventList& out)
{
    const int status = e.bytes[0] & 0xF0, channel = e.bytes[0] & 0x0F;
    const int data1 = e.bytes[1] & 0x7F, data2 = e.bytes[2] & 0x7F;

    if (status == 0x90 && data2 > 0)
    {
        srcActive = true;
        srcNote = data1;
        srcVelocity = data2;
        blocked = false;

        if (allowed (data1))
        {
            if (old.note >= 0)
                endVoice (old, offset, out);

            if (cur.note >= 0)
            {
                const bool sharedChannel = cfg.triad && ! mirror();
                if (cur.held || cur.note == data1 || sharedChannel)
                {
                    endVoice (cur, offset, out);      // off first, then the new note
                }
                else
                {
                    // Legato from the tracker: new note on, then its note-off for the old one.
                    endTriad (offset, out);
                    old = cur;
                    cur = Voice();
                }
            }
            startMain (data1, data2, data1, cfg.mpe ? allocChannel() : channel, offset, out);
        }
        else
        {
            blocked = true;
            if (cur.note >= 0 && ! cur.held)
            {
                if (canHold())
                    beginHold (offset, out);
                else
                    endVoice (cur, offset, out);
            }
        }
        return;
    }

    if (status == 0x80 || status == 0x90)
    {
        if (old.note >= 0 && old.src == data1)
        {
            endVoice (old, offset, out);
        }
        else if (cur.note >= 0 && ! cur.held && cur.src == data1)
        {
            if (canHold() && levelDb >= cfg.gateOffDb)
                beginHold (offset, out);
            else
                endVoice (cur, offset, out);
        }
        // else: this note was never sent

        if (data1 == srcNote)
        {
            srcActive = false;
            blocked = false;
        }
        return;
    }

    if (status == 0xE0)
    {
        srcBend14 = data1 | (data2 << 7);

        if (cur.note >= 0 && cur.held)
            return;                                   // R2: a held note is not bent

        if (cfg.mpe)
        {
            if (cur.note >= 0 && ! noteOnAhead)
                forwardBend (offset, out);            // R6: on the channel of the note it belongs to
            return;
        }

        if (cur.note >= 0 && cur.src != cur.note)
        {
            forwardBend (offset, out);                // R3: re-centred on the note that sounds
            return;
        }

        if (soloFreeze != Freeze::On)
            append (e, out);                          // R0: exactly as it came
        sentBend14 = srcBend14;
        mirrorBend (offset, out);
        return;
    }

    if (status == 0xB0)
    {
        const int k = dynIndex (data1);

        if (! cfg.mpe)
        {
            if (soloFreeze != Freeze::On)
                append (e, out);
        }
        else if (k < 0)
        {
            emit (Track::Solo, offset, 0xB0, 0, data1, data2, out);
        }
        else if (noteOnAhead)
        {
            srcPressure = data2;                      // sent in startMain, on the new channel
        }
        else if (cur.note >= 0)
        {
            emit (Track::Solo, offset, 0xB0, cur.channel, data1, data2, out);
            if (data2 != sentPressure)
                emit (Track::Solo, offset, 0xD0, cur.channel, data2, 0, out);
            sentPressure = srcPressure = data2;
        }
        else
        {
            // The tracker hands a controller back after the note: it goes where the note was.
            emit (Track::Solo, offset, 0xB0, lastMainChannel, data1, data2, out);
            srcPressure = -1;
        }

        if (k >= 0)
        {
            srcDyn[k] = sentDyn[k] = data2;
            mirrorDynamics (k, data2, offset, out);
        }
        return;
    }

    if (soloFreeze != Freeze::On)
        append (e, out);
}

//==============================================================================
void OutputStage::startMain (int note, int vel, int srcNoteFor, int channel, int offset, MidiEventList& out)
{
    velocity = clampi (vel, 1, 127);
    cur.note = note;
    cur.channel = channel & 0x0F;
    cur.src = srcNoteFor;
    cur.held = false;
    lastMainChannel = cur.channel;

    if (cfg.mpe)
    {
        // R6: a channel of its own. It is brought to the tracker's state before the note starts.
        sentBend14 = chanBend[cur.channel];

        for (int k = 0; k < kDynCount; ++k)
        {
            const int have = chanDyn[cur.channel][k] >= 0 ? chanDyn[cur.channel][k] : kDynNeutral[k];
            if (srcDyn[k] >= 0 && srcDyn[k] != have)
                emit (Track::Solo, offset, 0xB0, cur.channel, kDynNumber[k], srcDyn[k], out);
        }

        if (srcPressure >= 0 && chanPressure[cur.channel] != srcPressure)
            emit (Track::Solo, offset, 0xD0, cur.channel, srcPressure, 0, out);
        sentPressure = srcPressure;
    }

    forwardBend (offset, out);
    emit (Track::Solo, offset, 0x90, cur.channel, note, velocity, out);

    if (soloFreeze == Freeze::Armed)
        captureSolo (offset, out);                    // R7a

    beginHarmony (offset, out);
}

void OutputStage::endVoice (Voice& v, int offset, MidiEventList& out)
{
    if (v.note < 0)
        return;

    emit (Track::Solo, offset, 0x80, v.channel, v.note, 0, out);
    const bool wasCurrent = &v == &cur;
    v = Voice();

    if (wasCurrent)
        endTriad (offset, out);
}

void OutputStage::beginHold (int offset, MidiEventList& out)
{
    cur.held = true;

    // The note goes back to its own pitch at once, and so does a harmony that had followed the bend (R4c).
    const bool rootMoved = mirror() && harmActive && harmRoot != cur.note;
    if (rootMoved)
        endTriad (offset, out);

    if (sentBend14 != kCentre)
        sendBend (kCentre, offset, out);

    if (rootMoved)
    {
        harmRoot = cur.note;
        startTriad (offset, out);
    }
}

void OutputStage::sendBend (int value14, int offset, MidiEventList& out)
{
    value14 = clampi (value14, 0, 16383);
    const int channel = cur.note >= 0 ? cur.channel : cfg.midiChannel - 1;
    emit (Track::Solo, offset, 0xE0, channel, value14 & 0x7F, (value14 >> 7) & 0x7F, out);
    sentBend14 = value14;
    mirrorBend (offset, out);
}

void OutputStage::forwardBend (int offset, MidiEventList& out)
{
    int value = srcBend14;
    if (cur.note >= 0 && cur.src != cur.note)
    {
        const float semis = (float) (srcBend14 - kCentre) / 8192.0f * cfg.bendRangeSemis + (float) (cur.src - cur.note);
        value = clampi (kCentre + (int) std::lround (semis / cfg.bendRangeSemis * 8192.0f), 0, 16383);
    }

    if (value != sentBend14)
        sendBend (value, offset, out);
}

void OutputStage::syncSolo (int channel, int offset, MidiEventList& out)
{
    if (chanBend[channel] != sentBend14)
        emit (Track::Solo, offset, 0xE0, channel, sentBend14 & 0x7F, (sentBend14 >> 7) & 0x7F, out);

    for (int k = 0; k < kDynCount; ++k)
        if (sentDyn[k] >= 0 && chanDyn[channel][k] != sentDyn[k])
            emit (Track::Solo, offset, 0xB0, channel, kDynNumber[k], sentDyn[k], out);

    if (cfg.mpe && sentPressure >= 0 && chanPressure[channel] != sentPressure)
        emit (Track::Solo, offset, 0xD0, channel, sentPressure, 0, out);
}

//==============================================================================
// Harmony

float OutputStage::harmonyPitch() const noexcept
{
    if (cur.note < 0)
        return 0.0f;

    // A held note, or one the tracker has left, sounds where it was started.
    if (cur.held || ! srcActive || cur.src != srcNote)
        return (float) cur.note;

    return (float) srcNote + (float) (srcBend14 - kCentre) / 8192.0f * cfg.bendRangeSemis;
}

int OutputStage::harmonyBend() const noexcept
{
    if (cur.note < 0 || ! harmActive || harmRoot == cur.note)
        return sentBend14;                            // R4e: the same as on the main channel

    const float semis = harmonyPitch() - (float) harmRoot;
    return clampi (kCentre + (int) std::lround (semis / cfg.bendRangeSemis * 8192.0f), 0, 16383);
}

void OutputStage::mirrorBend (int offset, MidiEventList& out)
{
    if (! mirror())
        return;

    const bool ownRoot = harmActive && cur.note >= 0 && harmRoot != cur.note;
    const int value = harmonyBend();
    if (ownRoot && value == triadBend14)
        return;

    emit (Track::Harmony, offset, 0xE0, cfg.triadChannel - 1, value & 0x7F, (value >> 7) & 0x7F, out);
    triadBend14 = value;
    triadBendMoved = value != kCentre;
}

void OutputStage::mirrorDynamics (int k, int value, int offset, MidiEventList& out)
{
    if (! mirror())
        return;

    emit (Track::Harmony, offset, 0xB0, cfg.triadChannel - 1, kDynNumber[k], value, out);
    triadDyn[k] = value;
    triadDynMoved[k] = value != kDynNeutral[k];
}

void OutputStage::beginHarmony (int offset, MidiEventList& out)
{
    if (! cfg.triad || cur.note < 0)
        return;

    harmPending = false;
    harmWait = 0;
    harmCand = -1;
    harmCount = 0;

    if (! mirror())
    {
        harmRoot = cur.note;                          // R4f
        startTriad (offset, out);
        return;
    }

    const float pitch = harmonyPitch();
    const int nearest = clampi ((int) std::lround (pitch), 0, 127);

    if (std::abs (pitch - (float) nearest) <= kHarmonyClear)
    {
        harmRoot = nearest;
        startTriad (offset, out);
    }
    else
    {
        harmPending = true;                           // R4b: wait for the pitch to settle
    }
}

void OutputStage::updateHarmony (int offset, MidiEventList& out)
{
    if (! cfg.triad || cur.note < 0 || ! mirror())
        return;

    const float pitch = harmonyPitch();
    const int nearest = clampi ((int) std::lround (pitch), 0, 127);

    if (harmPending)
    {
        if (std::abs (pitch - (float) nearest) <= kHarmonyClear || ++harmWait >= settleHops)
        {
            harmPending = false;
            harmRoot = nearest;
            startTriad (offset, out);
        }
        return;
    }

    if (! harmActive)
        return;

    if (std::abs (pitch - (float) harmRoot) <= 0.5f + kHarmonyHysteresis)
    {
        harmCand = -1;
        harmCount = 0;
        return;
    }

    if (nearest != harmCand)
    {
        harmCand = nearest;
        harmCount = 0;
    }

    if (++harmCount >= settleHops)                    // R4c
    {
        endTriad (offset, out);
        harmRoot = nearest;
        harmCand = -1;
        harmCount = 0;
        startTriad (offset, out);
    }
}

void OutputStage::startTriad (int offset, MidiEventList& out)
{
    if (! cfg.triad || cur.note < 0 || harmActive)
        return;

    const bool ownChannel = mirror();
    const scale::Voices v = scale::voices (harmRoot, cfg.keyRoot, cfg.scaleMode, cfg.voicing,
                                           cfg.harmonyMode == HarmonyMode::Diatonic, cfg.freeVoicing, ! ownChannel);
    triadChan = cfg.triadChannel - 1;
    harmActive = true;

    if (ownChannel)
    {
        // The second channel starts at the same pitch and loudness as the first.
        // Compared with what the channel really has as well: it may have been bent by
        // something else (a main note before it, in mpe).
        const int bend = harmonyBend();
        if (triadBend14 != bend || (harmFreeze != Freeze::On && chanBend[triadChan] != bend))
        {
            emit (Track::Harmony, offset, 0xE0, triadChan, bend & 0x7F, (bend >> 7) & 0x7F, out);
            triadBend14 = bend;
            triadBendMoved = bend != kCentre;
        }

        for (int k = 0; k < kDynCount; ++k)
        {
            if (sentDyn[k] >= 0 && triadDyn[k] != sentDyn[k])
            {
                emit (Track::Harmony, offset, 0xB0, triadChan, kDynNumber[k], sentDyn[k], out);
                triadDyn[k] = sentDyn[k];
                triadDynMoved[k] = sentDyn[k] != kDynNeutral[k];
            }
        }
    }

    for (int i = 0; i < v.count; ++i)
    {
        emit (Track::Harmony, offset, 0x90, triadChan, v.notes[i], velocity, out);
        triadNotes[triadCount_++] = v.notes[i];
    }

    if (harmFreeze == Freeze::Armed && triadCount_ > 0)
        captureHarmony();                             // R7a
}

void OutputStage::endTriad (int offset, MidiEventList& out)
{
    for (int i = 0; i < triadCount_; ++i)
        emit (Track::Harmony, offset, 0x80, triadChan, triadNotes[i], 0, out);

    triadCount_ = 0;
    harmActive = false;
    harmPending = false;
}

void OutputStage::neutraliseTriadChannel (int channel, int offset, MidiEventList& out)
{
    if (triadBendMoved)
        emit (Track::Harmony, offset, 0xE0, channel, 0, 64, out);

    for (int k = 0; k < kDynCount; ++k)
    {
        if (triadDynMoved[k])
            emit (Track::Harmony, offset, 0xB0, channel, kDynNumber[k], kDynNeutral[k], out);

        triadDynMoved[k] = false;
        triadDyn[k] = -1;
    }

    triadBendMoved = false;
    triadBend14 = kCentre;
}

void OutputStage::handBackChannel (int channel, int offset, MidiEventList& out)
{
    if (chanBend[channel] != kCentre)
        emitAlways (offset, 0xE0, channel, 0, 64, out);

    for (int k = 0; k < kDynCount; ++k)
        if (chanDyn[channel][k] >= 0 && chanDyn[channel][k] != kDynNeutral[k])
            emitAlways (offset, 0xB0, channel, kDynNumber[k], kDynNeutral[k], out);
}

//==============================================================================
// Freeze (R7)

void OutputStage::captureSolo (int offset, MidiEventList& out)
{
    if (old.note >= 0)
        endVoice (old, offset, out);                  // its note-off would be swallowed otherwise

    soloDrone = cur;
    soloFreeze = Freeze::On;
}

void OutputStage::releaseSolo (int offset, MidiEventList& out)
{
    const bool wasOn = soloFreeze == Freeze::On;
    soloFreeze = Freeze::Off;
    if (! wasOn)
        return;

    const Voice drone = soloDrone;
    soloDrone = Voice();

    // Still (or again) on the frozen note: it simply carries on. So does a note the harmony has there (R7e).
    const bool same = cur.note >= 0 && cur.note == drone.note && cur.channel == drone.channel;
    if (! same && ! harmonyHas (drone.channel, drone.note))
        emitAlways (offset, 0x80, drone.channel, drone.note, 0, out);

    if (cur.note >= 0)
    {
        syncSolo (cur.channel, offset, out);
        if (! same)
            emit (Track::Solo, offset, 0x90, cur.channel, cur.note, velocity, out);
    }
    else if (! cfg.mpe)
    {
        syncSolo (cfg.midiChannel - 1, offset, out);
    }

    // What the harmony could not send on the frozen note's channel (R7e) is made up now.
    const int harmonyChannel = cfg.triadChannel - 1;
    if (mirror() && harmFreeze != Freeze::On && harmonyChannel == drone.channel)
    {
        if (chanBend[harmonyChannel] != triadBend14)
            emit (Track::Harmony, offset, 0xE0, harmonyChannel, triadBend14 & 0x7F, (triadBend14 >> 7) & 0x7F, out);

        for (int k = 0; k < kDynCount; ++k)
            if (triadDyn[k] >= 0 && chanDyn[harmonyChannel][k] != triadDyn[k])
                emit (Track::Harmony, offset, 0xB0, harmonyChannel, kDynNumber[k], triadDyn[k], out);
    }
}

void OutputStage::captureHarmony() noexcept
{
    harmDroneCount = triadCount_;
    harmDroneChan = triadChan;
    for (int i = 0; i < triadCount_; ++i)
        harmDrone[i] = triadNotes[i];

    harmFreeze = Freeze::On;
}

void OutputStage::releaseHarmony (int offset, MidiEventList& out)
{
    const bool wasOn = harmFreeze == Freeze::On;
    harmFreeze = Freeze::Off;
    if (! wasOn)
        return;

    bool same = triadCount_ > 0 && triadCount_ == harmDroneCount && triadChan == harmDroneChan;
    for (int i = 0; same && i < triadCount_; ++i)
        same = triadNotes[i] == harmDrone[i];

    if (! same)
        for (int i = 0; i < harmDroneCount; ++i)
            if (! soloHas (harmDroneChan, harmDrone[i]))   // R7e: the solo track has this very note
                emitAlways (offset, 0x80, harmDroneChan, harmDrone[i], 0, out);

    const int droneChannel = harmDroneChan;
    const int harmonyChannel = cfg.triadChannel - 1;
    harmDroneCount = 0;

    if (! (mirror() && droneChannel == harmonyChannel) && (cfg.mpe || droneChannel != cfg.midiChannel - 1))
        handBackChannel (droneChannel, offset, out);  // the harmony has left this channel meanwhile (R5b)

    if (mirror())
    {
        // The harmony's channel (the same or a new one): bring it to where the harmony is now (R7c).
        if (chanBend[harmonyChannel] != triadBend14)
            emitAlways (offset, 0xE0, harmonyChannel, triadBend14 & 0x7F, (triadBend14 >> 7) & 0x7F, out);

        for (int k = 0; k < kDynCount; ++k)
            if (triadDyn[k] >= 0 && chanDyn[harmonyChannel][k] != triadDyn[k])
                emitAlways (offset, 0xB0, harmonyChannel, kDynNumber[k], triadDyn[k], out);
    }

    if (! same)
        for (int i = 0; i < triadCount_; ++i)
            emit (Track::Harmony, offset, 0x90, triadChan, triadNotes[i], velocity, out);
}

//==============================================================================
// MPE (R6)

void OutputStage::sendMpeZone (int memberChannels, int offset, MidiEventList& out)
{
    emitAlways (offset, 0xB0, 0, 101, 0, out);
    emitAlways (offset, 0xB0, 0, 100, 6, out);
    emitAlways (offset, 0xB0, 0, 6, memberChannels, out);
}

bool OutputStage::channelBusy (int channel) const noexcept
{
    return (cur.note >= 0 && cur.channel == channel)
        || (old.note >= 0 && old.channel == channel)
        || (soloFreeze == Freeze::On && soloDrone.channel == channel)
        || (cfg.triad && channel == cfg.triadChannel - 1)
        || (harmFreeze == Freeze::On && harmDroneCount > 0 && harmDroneChan == channel);
}

int OutputStage::allocChannel() noexcept
{
    for (int tries = 0; tries < 15; ++tries)
    {
        const int channel = mpeNext;
        mpeNext = mpeNext >= 15 ? 1 : mpeNext + 1;
        if (! channelBusy (channel))
            return channel;
    }
    return mpeNext;   // not reached: at most five channels are ever in use
}

} // namespace vm
