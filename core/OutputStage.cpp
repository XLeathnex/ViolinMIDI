#include "OutputStage.h"

#include "Scale.h"

#include <algorithm>
#include <cmath>

namespace vm
{

namespace
{
    constexpr int    kCentre = 8192;
    constexpr int    kExpression = 11;
    constexpr double kSettleSec = 0.030;
    constexpr float  kSettleTolerance = 0.30f;

    inline void push (MidiEventList& out, int offset, int status, int channel, int data1, int data2)
    {
        MidiEvent e;
        e.sampleOffset = offset;
        e.size = 3;
        e.bytes[0] = (uint8_t) (status | (channel & 0x0F));
        e.bytes[1] = (uint8_t) (data1 & 0x7F);
        e.bytes[2] = (uint8_t) (data2 & 0x7F);
        out.push_back (e);
    }

    inline int   clampi (int v, int lo, int hi) noexcept       { return std::min (hi, std::max (lo, v)); }
    inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
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
    blocked = false;
    settleCount = 0;
    settleTarget = -1;
    sentBend14 = kCentre;
    sentExpression = -1;
    velocity = 100;
    triadCount_ = 0;
    triadBend14 = kCentre;
    triadExpression = -1;
    triadBendMoved = triadExpressionMoved = false;
}

bool OutputStage::allowed (int note) const noexcept
{
    return ! cfg.keyFilter || scale::contains (scaleMask, note);
}

//==============================================================================
void OutputStage::process (const MidiEvent* in, int numIn, float levelDb, const Settings& settings,
                           int offset, MidiEventList& out)
{
    applySettings (settings, offset, out);

    for (int i = 0; i < numIn; ++i)
        handle (in[i], levelDb, offset, out);

    // R1d: a held note lives on the bow. Below the threshold it ends.
    if (cur.note >= 0 && cur.held && levelDb < cfg.gateDb)
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
                startMain (target, srcVelocity, srcNote, cfg.midiChannel - 1, offset, out);
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
}

void OutputStage::releaseAll (int offset, MidiEventList& out)
{
    endVoice (old, offset, out);
    endVoice (cur, offset, out);
    endTriad (offset, out);

    if (haveCfg)
    {
        if (sentBend14 != kCentre)
            push (out, offset, 0xE0, cfg.midiChannel - 1, 0, 64);
        neutraliseTriadChannel (cfg.triadChannel - 1, offset, out);
    }

    srcActive = false;
    srcNote = -1;
    srcBend14 = kCentre;
    sentBend14 = kCentre;
    sentExpression = -1;
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
    n.bendRangeSemis = clampf (n.bendRangeSemis, 0.1f, 96.0f);

    if (! haveCfg)
    {
        cfg = n;
        haveCfg = true;
        scaleMask = scale::mask (cfg.keyRoot, cfg.scaleMode, cfg.pentatonic);
        return;
    }

    const bool keyChanged = n.keyFilter != cfg.keyFilter || n.keyRoot != cfg.keyRoot || n.scaleMode != cfg.scaleMode
                         || n.pentatonic != cfg.pentatonic || n.pentaSustain != cfg.pentaSustain;
    const bool triadChanged = n.triad != cfg.triad || n.triadChannel != cfg.triadChannel || n.triadWithRoot != cfg.triadWithRoot;
    const bool channelChanged = n.midiChannel != cfg.midiChannel;

    if (! keyChanged && ! triadChanged && ! channelChanged)
    {
        cfg = n;
        return;
    }

    // What was started under the old settings ends on the old channels.
    if (triadChanged || channelChanged)
    {
        endTriad (offset, out);

        // The second channel gets its controllers back when it is left.
        const bool mirrorsAfter = n.triad && n.triadChannel != n.midiChannel;
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

    // On a channel change the tracker ends its note in this very hop: no new triad for it.
    if (cur.note >= 0 && ! channelChanged)
    {
        if (! cfg.triad)
        {
            endTriad (offset, out);
        }
        else
        {
            const bool sameChannel = cfg.triadChannel == cfg.midiChannel;
            const scale::Triad wanted = scale::triad (cur.note, cfg.keyRoot, cfg.scaleMode, cfg.triadWithRoot && ! sameChannel);
            bool same = wanted.count == triadCount_ && triadChan == cfg.triadChannel - 1;
            for (int i = 0; same && i < wanted.count; ++i)
                same = wanted.notes[i] == triadNotes[i];

            if (! same)
            {
                endTriad (offset, out);
                startTriad (offset, out);
            }
        }
    }
}

//==============================================================================
void OutputStage::handle (const MidiEvent& e, float levelDb, int offset, MidiEventList& out)
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
                const bool sameChannelTriad = cfg.triad && cfg.triadChannel == cfg.midiChannel;
                if (cur.held || cur.note == data1 || sameChannelTriad)
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
            startMain (data1, data2, data1, channel, offset, out);
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
            if (canHold() && levelDb >= cfg.gateDb)
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

        if (cur.note >= 0 && cur.src != cur.note)
        {
            forwardBend (offset, out);                // R3: re-centred on the note that sounds
            return;
        }

        out.push_back (e);
        sentBend14 = srcBend14;
        if (mirror())
        {
            push (out, offset, 0xE0, cfg.triadChannel - 1, data1, data2);
            triadBend14 = srcBend14;
            triadBendMoved = srcBend14 != kCentre;
        }
        return;
    }

    out.push_back (e);

    if (status == 0xB0 && data1 == kExpression)
    {
        sentExpression = data2;
        if (mirror())
        {
            push (out, offset, 0xB0, cfg.triadChannel - 1, kExpression, data2);
            triadExpression = data2;
            triadExpressionMoved = data2 != 127;
        }
    }
}

//==============================================================================
void OutputStage::startMain (int note, int vel, int srcNoteFor, int channel, int offset, MidiEventList& out)
{
    velocity = clampi (vel, 1, 127);
    cur.note = note;
    cur.channel = channel & 0x0F;
    cur.src = srcNoteFor;
    cur.held = false;

    forwardBend (offset, out);
    push (out, offset, 0x90, cur.channel, note, velocity);
    startTriad (offset, out);
}

void OutputStage::endVoice (Voice& v, int offset, MidiEventList& out)
{
    if (v.note < 0)
        return;

    push (out, offset, 0x80, v.channel, v.note, 0);
    const bool wasCurrent = &v == &cur;
    v = Voice();

    if (wasCurrent)
        endTriad (offset, out);
}

void OutputStage::beginHold (int offset, MidiEventList& out)
{
    cur.held = true;
    if (sentBend14 != kCentre)
        sendBend (kCentre, offset, out);
}

void OutputStage::startTriad (int offset, MidiEventList& out)
{
    if (! cfg.triad || cur.note < 0 || triadCount_ > 0)
        return;

    const bool sameChannel = cfg.triadChannel == cfg.midiChannel;
    const scale::Triad t = scale::triad (cur.note, cfg.keyRoot, cfg.scaleMode, cfg.triadWithRoot && ! sameChannel);
    triadChan = cfg.triadChannel - 1;

    if (mirror())
    {
        // The second channel starts at the same pitch and loudness as the first.
        if (triadBend14 != sentBend14)
        {
            push (out, offset, 0xE0, triadChan, sentBend14 & 0x7F, (sentBend14 >> 7) & 0x7F);
            triadBend14 = sentBend14;
            triadBendMoved = sentBend14 != kCentre;
        }
        if (sentExpression >= 0 && triadExpression != sentExpression)
        {
            push (out, offset, 0xB0, triadChan, kExpression, sentExpression);
            triadExpression = sentExpression;
            triadExpressionMoved = sentExpression != 127;
        }
    }

    for (int i = 0; i < t.count; ++i)
    {
        push (out, offset, 0x90, triadChan, t.notes[i], velocity);
        triadNotes[triadCount_++] = t.notes[i];
    }
}

void OutputStage::endTriad (int offset, MidiEventList& out)
{
    for (int i = 0; i < triadCount_; ++i)
        push (out, offset, 0x80, triadChan, triadNotes[i], 0);
    triadCount_ = 0;
}

void OutputStage::neutraliseTriadChannel (int channel, int offset, MidiEventList& out)
{
    if (triadBendMoved)
        push (out, offset, 0xE0, channel, 0, 64);
    if (triadExpressionMoved)
        push (out, offset, 0xB0, channel, kExpression, 127);

    triadBendMoved = triadExpressionMoved = false;
    triadBend14 = kCentre;
    triadExpression = -1;
}

void OutputStage::sendBend (int value14, int offset, MidiEventList& out)
{
    value14 = clampi (value14, 0, 16383);
    const int channel = cur.note >= 0 ? cur.channel : cfg.midiChannel - 1;
    push (out, offset, 0xE0, channel, value14 & 0x7F, (value14 >> 7) & 0x7F);
    sentBend14 = value14;

    if (mirror())
    {
        push (out, offset, 0xE0, cfg.triadChannel - 1, value14 & 0x7F, (value14 >> 7) & 0x7F);
        triadBend14 = value14;
        triadBendMoved = value14 != kCentre;
    }
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

} // namespace vm
