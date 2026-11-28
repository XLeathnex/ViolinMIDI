#include "NoteTracker.h"

#include <algorithm>
#include <cmath>

namespace vm
{

namespace
{
    // --- onset
    constexpr float kOnsetTolerance = 0.4f;      // a starting note must stay this close to where it began

    // --- finger step versus slide
    // A slide moves a little on every hop; a finger change makes the estimate
    // jump. Anything faster than about 100 semitones per second is a step.
    constexpr float kJumpPerHop       = 0.30f;   // semitones between two hops
    constexpr float kJumpWindow       = 0.60f;   // semitones across the short history
    constexpr float kStableTolerance  = 0.30f;   // "settled" means staying within this of where it landed
    // While the comparison window still holds the end of the old note, the
    // estimate can rest on a meaningless in-between value for up to one period.
    // A new pitch therefore has to hold for about one period of its own before
    // it is believed. Transitions may take this many periods at most.
    constexpr float kTransitionMaxPeriods = 3.5f;
    constexpr float kNewNoteInterval  = 0.60f;   // a settled step this large is a new note
    constexpr float kOctaveTolerance  = 0.40f;

    // --- playing aids
    // The slur hold: a slur down to the lowest string leaves the detector
    // without a pitch for about this many periods of that string. The note is
    // always held that long, whatever Settings::bridgeMs says.
    constexpr float kSlurHoldPeriods  = 2.6f;
    // Jump guard: a leap counts as wide from half a semitone below
    // Settings::jumpIntervalSemis on, so a slightly flat fifth is still a fifth
    // (the interval rounded to the nearest semitone is compared).
    constexpr float kJumpIntervalSlack = 0.5f;

    // --- bend modes
    constexpr float kQuantizeHysteresis = 0.15f; // semitones past the half-way point
    constexpr float kVibratoDeadband    = 0.40f; // vibrato up to +-40 cents is removed
    constexpr double kVibratoWindowMaxSec = 0.36; // at vibratoSmoothing = 1; the default 0.5 gives 180 ms

    // --- level
    // A note starts above Settings::gateDb ("in") and ends below Settings::gateOffDb ("out").
    constexpr float kRelativeDropDb   = 30.0f;   // ...or this far below its own peak
    constexpr float kDipDb            = 9.0f;    // a new bow stroke: level dips this much...
    constexpr float kRiseDb           = 6.0f;    // ...and comes back up by this much
    constexpr float kCollapseDb       = 12.0f;   // this far below the note's own level: the string is dying away
    constexpr float kLegatoPeakDropDb = 6.0f;    // a slurred note may be this much softer than the one before
    constexpr float kAttackRisingDb   = 1.0f;    // level still climbing by this much: the attack is not over
    constexpr float kAfterNoteDropDb  = 18.0f;   // right after a note, a new one must be this close in level...
    constexpr float kAttackRiseDb     = 6.0f;    // ...or show a real attack
    constexpr float kSameNoteRiseDb   = 10.0f;   // the same pitch again needs a clearer attack
    constexpr float kSameNoteInterval = 0.7f;
    // An estimate one or more octaves below the sounding note is not believed
    // while the signal is still this periodic at the note's own period.
    constexpr float kSubharmonicHintClarity = 0.5f;
    // Quantized mode: a start further than this from the centre of a semitone
    // is unclear. Bowed notes start flat, so the decision waits a moment and
    // then leans upwards.
    constexpr float kUnclearDistance  = 0.25f;
    constexpr float kFlatAttackBias   = 0.10f;
    constexpr double kAfterNoteDecayDbPerSec = 20.0;
    constexpr float kDynFloorDb       = -54.0f;  // level mapped to the softest value
    constexpr float kDynCeilDb        = -12.0f;  // level mapped to the loudest value
    constexpr int   kExpressionFloor  = 40;
    constexpr int   kBendCentre       = 8192;

    // --- controllers that may carry the bow loudness (Settings::dynamicsTarget)
    // Sent in this order; each is handed back to its neutral value.
    constexpr unsigned kOwnModWheel   = 1u;   // CC 1  -> 0
    constexpr unsigned kOwnBreath     = 2u;   // CC 2  -> 127
    constexpr unsigned kOwnExpression = 4u;   // CC 11 -> 127
    struct LoudnessController { unsigned bit; int number; int neutral; };
    constexpr LoudnessController kLoudnessControllers[] = {
        { kOwnModWheel,    1,  0   },
        { kOwnBreath,      2,  127 },
        { kOwnExpression,  11, 127 }
    };

    inline unsigned controllersFor (DynamicsTarget t) noexcept
    {
        switch (t)
        {
            case DynamicsTarget::ModWheel:         return kOwnModWheel;
            case DynamicsTarget::Breath:           return kOwnBreath;
            case DynamicsTarget::ModAndExpression: return kOwnModWheel | kOwnExpression;
            case DynamicsTarget::Expression:       break;
        }
        return kOwnExpression;
    }

    // --- articulation (Settings::articulation)
    // The steepness of a start is how fast the level climbed through the band
    // from 36 dB to 12 dB below its value at the note-on, in dB per ms. The
    // band sits low because a bowed note is often started while its level is
    // still rising; what tells a pluck from a bow is how it leaves the floor
    // (a pluck jumps 30..50 dB within one or two hops, a bow climbs 5..10 dB
    // per hop). Measured with tests/Synth.h at 750 hops/s, notes 57..93,
    // -14 and -28 dB: plucks (2 ms attack) 10.8 .. 20 dB/ms, bowed starts
    // (20 and 40 ms bow attack, notes 45..93) 2.0 .. 6.4 dB/ms. Low strings
    // read slower, since the level is measured over whole periods: a loud
    // pluck on A2 (45) measures about 4.5 dB/ms.
    constexpr float kArticBandLowDb     = 36.0f;
    constexpr float kArticBandHighDb    = 12.0f;
    constexpr float kArticFloorMarginDb = 3.0f;  // a high noise floor: the band starts this far above it
    constexpr float kArticMinBandDb     = 12.0f; // less rise than this out of the floor: a soft start
    constexpr float kArticSoftDbPerMs   = 7.5f;  // this slow or slower: soft start, 45..65
    constexpr float kArticSteepDbPerMs  = 9.5f;  // this steep or steeper: hard start, 95..127
    constexpr int   kArticSoftLow  = 45, kArticSoftHigh  = 65;
    constexpr int   kArticSteepLow = 95, kArticSteepHigh = 127;

    inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
    inline int   clampi (int v, int lo, int hi) noexcept       { return std::min (hi, std::max (lo, v)); }

    inline bool isOctaveLike (float interval) noexcept
    {
        return std::abs (interval - 12.0f) < kOctaveTolerance
            || std::abs (interval - 19.02f) < kOctaveTolerance
            || std::abs (interval - 24.0f) < kOctaveTolerance;
    }

    inline int hopsFor (double seconds, double hopRate, int minimum) noexcept
    {
        return std::max (minimum, (int) std::lround (seconds * hopRate));
    }

    /** Hops for an octave guard: none when off or at 0 ms. The defaults (50 / 11 ms)
        give exactly the former fixed hopsFor (0.050 / 0.011, hopRate, 3). */
    inline int guardHops (bool on, float ms, double hopRate) noexcept
    {
        if (! on || ! (ms > 0.0f))
            return 0;
        return hopsFor ((double) ms / 1000.0, hopRate, 3);
    }

    /** Hops a note is bridged through a gap in detection: none at 0 ms. The
        default 30 ms gives exactly the former fixed hopsFor (0.030, hopRate, 2). */
    inline int bridgeHopsFor (float ms, double hopRate) noexcept
    {
        if (! (ms > 0.0f))
            return 0;
        return hopsFor ((double) ms / 1000.0, hopRate, 2);
    }

    Settings sanitised (Settings s) noexcept
    {
        s.bendRangeSemis = clampf (s.bendRangeSemis, 0.1f, 96.0f);
        s.vibratoSmoothing = clampf (s.vibratoSmoothing, 0.0f, 1.0f);
        s.lowestNote     = clampi (s.lowestNote, kAbsLowestNote, kAbsHighestNote - 12);
        s.highestNote    = clampi (s.highestNote, s.lowestNote + 12, kAbsHighestNote);
        s.fixedVelocity  = clampi (s.fixedVelocity, 1, 127);
        s.midiChannel    = clampi (s.midiChannel, 1, 16);
        s.gateDb         = clampf (s.gateDb, -90.0f, 0.0f);
        s.gateOffDb      = std::min (clampf (s.gateOffDb, -96.0f, 0.0f), s.gateDb); // out never above in
        s.octaveAttackMs  = clampf (s.octaveAttackMs, 0.0f, 60.0f);
        s.octaveSustainMs = clampf (s.octaveSustainMs, 0.0f, 100.0f);
        s.jumpIntervalSemis = clampi (s.jumpIntervalSemis, 5, 24);
        s.jumpWindowMs    = clampf (s.jumpWindowMs, 10.0f, 200.0f);
        s.jumpLockMs      = clampf (s.jumpLockMs, 20.0f, 150.0f);
        s.bridgeMs        = clampf (s.bridgeMs, 0.0f, 200.0f);
        s.dynamicsSmoothingMs = clampf (s.dynamicsSmoothingMs, 0.0f, 150.0f);
        if ((int) s.articulation < 0 || (int) s.articulation > 2)
            s.articulation = Articulation::Off;
        if ((int) s.dynamicsTarget < 0 || (int) s.dynamicsTarget > 3)
            s.dynamicsTarget = DynamicsTarget::Expression;
        s.a4Hz           = clampf (s.a4Hz, kMinA4Hz, kMaxA4Hz);
        return s;
    }
}

void NoteTracker::prepare (double hopsPerSecond)
{
    hopRate = std::max (50.0, hopsPerSecond);

    holdHops          = hopsFor (0.030, hopRate, 2);
    quietHopsNeeded   = hopsFor (0.003, hopRate, 2);
    octaveStableHops  = hopsFor (0.011, hopRate, 3);
    transitionMaxHops = hopsFor (0.025, hopRate, 4);
    minRetriggerHops  = hopsFor (0.040, hopRate, 4);
    ccMinHops         = hopsFor (0.005, hopRate, 1);
    doubtHops         = hopsFor (0.050, hopRate, 3);
    subharmonicGuardHops = hopsFor (0.100, hopRate, 4);
    sameNoteHops      = hopsFor (0.400, hopRate, 4);
    attackWindowHops  = hopsFor (0.070, hopRate, 4);
    levelAgoHops      = std::min (kLevelHistory - 2, hopsFor (0.015, hopRate, 1));
    deferMaxHops      = hopsFor (0.045, hopRate, 2);

    peakDecayPerHop   = (float) (3.0 / hopRate);   // dB per hop, 3 dB per second
    recentDecayPerHop = (float) (60.0 / hopRate);  // 60 dB per second
    onsetGateDecayPerHop = (float) (kAfterNoteDecayDbPerSec / hopRate);
    exprAttack  = (float) (1.0 - std::exp (-1.0 / (0.008 * hopRate)));
    exprRelease = (float) (1.0 - std::exp (-1.0 / (0.060 * hopRate)));

    smoothBuffer.assign ((size_t) hopsFor (kVibratoWindowMaxSec, hopRate, 1), 0.0f);
    smoothLen = hopsFor (0.5 * kVibratoWindowMaxSec, hopRate, 1);
    haveCfg = false;
    reset();
}

unsigned NoteTracker::attacks() const noexcept
{
    return 0;
}

unsigned NoteTracker::cueStrokes() const noexcept
{
    return 0;
}

void NoteTracker::reset() noexcept
{
    active = false;
    inTransition = false;
    jumpWatch = false;
    candCount = 0;
    unvoicedHops = 0;
    quietHops = 0;
    noteAgeHops = 0;
    dipArmed = false;
    notePeakDb = recentMaxDb = expressionDb = exprSmoothDb = lastLevelDb = -120.0f;
    onsetGateDb = -200.0f;
    idleMinDb = -200.0f;
    hopsSinceNoteEnd = 1000000;
    lastEndedPitch = -100.0f;
    deferHops = 0;
    hopsSinceAttack = 1000000;
    for (float& l : levelHistory)
        l = -120.0f;
    levelHistoryPos = 0;
    for (float& l : riseHistory)
        l = -120.0f;
    riseHistoryPos = 0;
    lastBend14 = -1;
    lastExpression = -1;
    hopsSinceExpression = 1000;
    ownedControllers = 0;
    resetSmoother (60.0f);
    fillHistory (60.0f);
}

//==============================================================================
void NoteTracker::process (const Input& in, const Settings& settings, int offset, MidiEventList& out)
{
    applySettings (sanitised (settings), offset, out);
    updateExpression (in.levelDb, offset, out, ExprUpdate::Follow);
    lastLevelDb = in.levelDb;
    levelHistory[levelHistoryPos] = in.levelDb;
    levelHistoryPos = (levelHistoryPos + 1) % kLevelHistory;
    riseHistory[riseHistoryPos] = in.levelDb;
    riseHistoryPos = (riseHistoryPos + 1) % kRiseHistory;

    if (active)
        processActive (in, offset, out);
    else
        processIdle (in, offset, out);
}

void NoteTracker::shutdown (int offset, MidiEventList& out)
{
    if (active)
        endNote (offset, out);

    if (haveCfg)
    {
        if (lastBend14 != kBendCentre && lastBend14 >= 0)
            sendBendValue (kBendCentre, offset, out);
        handBackControllers (ownedControllers, offset, out);
    }

    reset();
}

void NoteTracker::applySettings (const Settings& s, int offset, MidiEventList& out)
{
    const bool first = ! haveCfg;

    if (! first && s.midiChannel != cfg.midiChannel)
    {
        // Leave the old channel clean before moving on.
        if (active)
            endNote (offset, out);
        if (lastBend14 != kBendCentre && lastBend14 >= 0)
            sendBendValue (kBendCentre, offset, out);
        handBackControllers (ownedControllers, offset, out);
        lastBend14 = -1;
        lastExpression = -1;
    }

    if (! first && cfg.dynamics == DynamicsMode::Continuous && s.dynamics != DynamicsMode::Continuous)
    {
        // Hand the instrument's volume back, otherwise it would stay quiet.
        handBackControllers (ownedControllers, offset, out);
        lastExpression = -1;
    }

    if (! first && s.dynamicsTarget != cfg.dynamicsTarget)
    {
        // Controllers no longer used go back at once; the new ones carry the
        // level from the next update on.
        handBackControllers (ownedControllers & ~controllersFor (s.dynamicsTarget), offset, out);
        lastExpression = -1;
    }

    const bool rangeOrTuningChanged = first || s.lowestNote != cfg.lowestNote || s.a4Hz != cfg.a4Hz;
    const bool modeChanged  = first || s.bendMode != cfg.bendMode;
    const bool rangeChanged = first || s.bendRangeSemis != cfg.bendRangeSemis;
    const bool responseChanged = first || s.response != cfg.response;
    const bool smoothingChanged = first || s.vibratoSmoothing != cfg.vibratoSmoothing;
    const bool guardsChanged = first || s.octaveAttackGuard != cfg.octaveAttackGuard
                            || s.octaveAttackMs != cfg.octaveAttackMs
                            || s.octaveSustainGuard != cfg.octaveSustainGuard
                            || s.octaveSustainMs != cfg.octaveSustainMs;
    const bool exprSmoothingChanged = first || s.dynamicsSmoothingMs != cfg.dynamicsSmoothingMs;
    const bool bridgeChanged = rangeOrTuningChanged || s.bridgeMs != cfg.bridgeMs;
    const bool jumpTimesChanged = first || s.jumpWindowMs != cfg.jumpWindowMs || s.jumpLockMs != cfg.jumpLockMs;

    cfg = s;
    haveCfg = true;

    if (bridgeChanged)
    {
        // Bridging: the note survives a gap in detection (a change of bow, a
        // scratch) for bridgeMs while the level holds. A slur down to the lowest
        // string leaves the detector without a pitch for about two and a half
        // periods of that string; the note is held at least that long, so the
        // slur stays a slur. The default 30 ms is the former fixed hold.
        holdHops = std::max (bridgeHopsFor (cfg.bridgeMs, hopRate),
                             (int) std::ceil (kSlurHoldPeriods * periodInHops ((float) cfg.lowestNote)));
    }

    if (jumpTimesChanged)
    {
        jumpWindowHops = hopsFor ((double) cfg.jumpWindowMs / 1000.0, hopRate, 1);
        jumpLockHops   = hopsFor ((double) cfg.jumpLockMs / 1000.0, hopRate, 1);
    }

    if (responseChanged)
    {
        switch (cfg.response)
        {
            case Response::Fast:   confirmHops = 2; onsetClarity = 0.75f; stableHops = 2; holdPeriods = 0.60f; onsetPeriods = 0.0f; break;
            case Response::Normal: confirmHops = 3; onsetClarity = 0.80f; stableHops = 2; holdPeriods = 1.15f; onsetPeriods = 1.0f; break;
            case Response::Safe:   confirmHops = 5; onsetClarity = 0.86f; stableHops = 3; holdPeriods = 1.50f; onsetPeriods = 2.0f; break;
        }
        sustainClarity = 0.55f;
    }

    if (guardsChanged)
    {
        doubtHops        = guardHops (cfg.octaveAttackGuard, cfg.octaveAttackMs, hopRate);
        octaveStableHops = guardHops (cfg.octaveSustainGuard, cfg.octaveSustainMs, hopRate);
    }

    if (exprSmoothingChanged)
    {
        exprSmoothCoeff = cfg.dynamicsSmoothingMs > 0.0f
                            ? (float) (1.0 - std::exp (-1.0 / ((double) cfg.dynamicsSmoothingMs * 0.001 * hopRate)))
                            : 1.0f;
        exprSmoothDb = expressionDb;
    }

    if (smoothingChanged)
    {
        smoothLen = std::min ((int) smoothBuffer.size(),
                              hopsFor ((double) cfg.vibratoSmoothing * kVibratoWindowMaxSec, hopRate, 1));
        resetSmoother (active ? lastPitch : 60.0f);
    }

    if (modeChanged)
    {
        if (active)
            resetSmoother (lastPitch);
        if (cfg.bendMode == BendMode::Quantized || ! active)
            sendBendValue (kBendCentre, offset, out);
    }

    if ((modeChanged || rangeChanged) && active && cfg.bendMode != BendMode::Quantized)
    {
        outPitch = lastPitch;
        sendBend (outPitch - (float) curNote, offset, out);
    }
}

//==============================================================================
void NoteTracker::updateExpression (float levelDb, int offset, MidiEventList& out, ExprUpdate kind)
{
    if (hopsSinceExpression < 100000)
        ++hopsSinceExpression;

    const bool force = kind != ExprUpdate::Follow;
    levelDb = clampf (levelDb, -120.0f, 20.0f);
    if (! force)
        expressionDb += (levelDb - expressionDb) * (levelDb > expressionDb ? exprAttack : exprRelease);
    else
        expressionDb = std::max (expressionDb, levelDb);

    // Optional extra smoothing (dynamicsSmoothingMs). A new note starts from
    // its own level; a finger change only lifts it, like expressionDb above.
    float driveDb = expressionDb;
    if (exprSmoothCoeff < 1.0f)
    {
        if (kind == ExprUpdate::Seed)
            exprSmoothDb = expressionDb;
        else if (kind == ExprUpdate::Raise)
            exprSmoothDb = std::max (exprSmoothDb, expressionDb);
        else
            exprSmoothDb += (expressionDb - exprSmoothDb) * exprSmoothCoeff;
        driveDb = exprSmoothDb;
    }
    else
    {
        exprSmoothDb = expressionDb;
    }

    if (cfg.dynamics != DynamicsMode::Continuous)
        return;

    const float x = clampf ((driveDb - kDynFloorDb) / (kDynCeilDb - kDynFloorDb), 0.0f, 1.0f);
    const int value = kExpressionFloor + (int) std::lround (x * (float) (127 - kExpressionFloor));

    if (value != lastExpression && (force || hopsSinceExpression >= ccMinHops))
    {
        const unsigned targets = controllersFor (cfg.dynamicsTarget);
        for (const LoudnessController& c : kLoudnessControllers)
            if ((targets & c.bit) != 0)
                sendController (c.number, value, offset, out);
        lastExpression = value;
        hopsSinceExpression = 0;
        ownedControllers |= targets;
    }
}

void NoteTracker::handBackControllers (unsigned mask, int offset, MidiEventList& out)
{
    for (const LoudnessController& c : kLoudnessControllers)
        if ((mask & ownedControllers & c.bit) != 0)
            sendController (c.number, c.neutral, offset, out);
    ownedControllers &= ~mask;
}

int NoteTracker::velocityFor (float levelDb) const noexcept
{
    if (cfg.dynamics == DynamicsMode::Fixed)
        return cfg.fixedVelocity;

    const float x = clampf ((levelDb - kDynFloorDb) / (kDynCeilDb - kDynFloorDb), 0.0f, 1.0f);
    return clampi (1 + (int) std::lround (x * 126.0f), 1, 127);
}

float NoteTracker::attackSlopeDbPerMs() const noexcept
{
    // How fast did the level climb through the band kArticBandLowDb ..
    // kArticBandHighDb below the level at the note-on? Crossing times are
    // interpolated between hops. A start that does not come up from below the
    // band within the memory (a re-bow out of a dip, a rise out of ringing)
    // counts as soft.
    auto level = [this] (int j) { return riseHistory[(riseHistoryPos + j) % kRiseHistory]; }; // 0 = oldest
    const int n = kRiseHistory;
    const float now = level (n - 1);
    float floorDb = now;
    for (int j = 0; j < n; ++j)
        floorDb = std::min (floorDb, level (j));
    // Over a noise floor less than 36 dB down, the band starts just above that floor.
    const float lo = std::max (now - kArticBandLowDb, floorDb + kArticFloorMarginDb);
    const float hi = now - kArticBandHighDb;
    if (hi - lo < kArticMinBandDb)
        return 0.0f;

    int iLo = -1;
    for (int j = n - 2; j >= 0; --j)
        if (level (j) <= lo) { iLo = j; break; }
    if (iLo < 0)
        return 0.0f;

    int iHi = -1;
    for (int j = iLo + 1; j < n; ++j)
        if (level (j) >= hi) { iHi = j; break; }
    if (iHi < 0)
        return 0.0f;

    auto crossing = [&level] (int j, float threshold) // between hop j and j + 1
    {
        const float a = level (j), b = level (j + 1);
        return (float) j + (b > a ? clampf ((threshold - a) / (b - a), 0.0f, 1.0f) : 0.0f);
    };
    const float hops = std::max (0.25f, crossing (iHi - 1, hi) - crossing (iLo, lo));
    return (hi - lo) / (float) ((double) hops * 1000.0 / hopRate);
}

int NoteTracker::noteOnVelocity (float levelDb, bool softStart) const noexcept
{
    const int plain = velocityFor (levelDb);
    if (cfg.dynamics == DynamicsMode::Fixed || cfg.articulation == Articulation::Off)
        return plain;

    // Steepness picks the range, loudness the place inside it.
    const float steep = softStart ? 0.0f
                                  : clampf ((attackSlopeDbPerMs() - kArticSoftDbPerMs)
                                              / (kArticSteepDbPerMs - kArticSoftDbPerMs), 0.0f, 1.0f);
    const float low  = (float) kArticSoftLow  + steep * (float) (kArticSteepLow  - kArticSoftLow);
    const float high = (float) kArticSoftHigh + steep * (float) (kArticSteepHigh - kArticSoftHigh);
    const float loud = clampf ((levelDb - kDynFloorDb) / (kDynCeilDb - kDynFloorDb), 0.0f, 1.0f);
    const int strong = clampi ((int) std::lround (low + loud * (high - low)), 1, 127);

    if (cfg.articulation == Articulation::Strong)
        return strong;
    return clampi ((plain + strong + 1) / 2, 1, 127);
}

float NoteTracker::periodInHops (float pitch) const noexcept
{
    const double hz = (double) cfg.a4Hz * std::pow (2.0, ((double) pitch - 69.0) / 12.0);
    return (float) (hopRate / std::max (20.0, hz));
}

bool NoteTracker::attackInProgress() const noexcept
{
    if (hopsSinceAttack >= attackWindowHops)
        return false;
    const int index = ((levelHistoryPos - 1 - levelAgoHops) % kLevelHistory + kLevelHistory) % kLevelHistory;
    return lastLevelDb > levelHistory[index] + kAttackRisingDb;
}

int NoteTracker::clampNote (int n) const noexcept
{
    return clampi (n, cfg.lowestNote, cfg.highestNote);
}

//==============================================================================
void NoteTracker::processIdle (const Input& in, int offset, MidiEventList& out)
{
    // Right after a note the string and its neighbours are still ringing.
    // That ringing is quieter than the note was and has no attack, so a new
    // note must either be nearly as loud or rise out of what was left.
    if (hopsSinceNoteEnd < 1000000)
        ++hopsSinceNoteEnd;
    onsetGateDb -= onsetGateDecayPerHop;
    idleMinDb = std::min (idleMinDb, in.levelDb);
    bool loudEnough = in.levelDb >= cfg.gateDb
                   && (in.levelDb >= onsetGateDb || in.levelDb >= idleMinDb + kAttackRiseDb);

    // The same pitch again right after its note ended is usually the tail
    // flickering around the gate, and its octave is an overtone outlasting the
    // fundamental. A real new stroke rises clearly out of what was left.
    if (in.voiced && hopsSinceNoteEnd < sameNoteHops)
    {
        const float interval = in.pitch - lastEndedPitch;
        const bool samePitch = std::abs (interval) < kSameNoteInterval;
        const bool overtone = interval > 0.0f && isOctaveLike (interval); // overtones lie above, never below
        if ((samePitch || overtone) && in.levelDb < idleMinDb + kSameNoteRiseDb)
            loudEnough = false;
    }

    const bool inRange = in.pitch >= (float) cfg.lowestNote - 0.5f
                      && in.pitch <= (float) cfg.highestNote + 0.5f;
    const bool ok = in.voiced && inRange
                 && in.clarity >= onsetClarity
                 && loudEnough;

    if (! ok)
    {
        candCount = 0;
        deferHops = 0;
        return;
    }

    if (candCount > 0 && std::abs (in.pitch - candPitch) <= kOnsetTolerance)
    {
        ++candCount;
    }
    else
    {
        candCount = 1;
        candPitch = in.pitch;
    }

    // While the bow bites, a string (a low one above all) wanders in pitch for
    // some tens of milliseconds. The note starts once the pitch has held still
    // for about one period. Shortly after another note its tail is still
    // inside the comparison window, so the pitch must hold even longer.
    const float periodHops = periodInHops (in.pitch);
    int needed = std::max (confirmHops, (int) std::ceil (onsetPeriods * periodHops));
    if ((float) hopsSinceNoteEnd < 3.0f * periodHops + (float) confirmHops)
        needed = std::max (needed, (int) std::ceil (holdPeriods * periodHops));
    if (in.octaveDoubt)
        needed = std::max (needed, doubtHops); // might be an overtone squeak: wait and see (0: guard off)
    if (cfg.bendMode == BendMode::Quantized)
        needed = std::max (needed, (int) std::ceil (2.0f * periodHops) + 2); // no bend to correct it later

    if (candCount < needed)
        return;

    float pitch = in.pitch;
    if (cfg.bendMode == BendMode::Quantized)
    {
        // Without pitch bend the choice of semitone is final, so an unclear
        // start is given a moment to settle.
        const float distance = std::abs (pitch - std::round (pitch));
        if (distance > kUnclearDistance && deferHops < deferMaxHops)
        {
            ++deferHops;
            return;
        }
        if (deferHops > 0)
            pitch += kFlatAttackBias;
    }

    deferHops = 0;
    startNote (pitch, in.levelDb, offset, out);
}

void NoteTracker::startNote (float pitch, float levelDb, int offset, MidiEventList& out)
{
    curChannel = cfg.midiChannel - 1;
    curNote = clampNote ((int) std::lround (pitch));
    lastPitch = pitch;
    fillHistory (pitch);
    resetSmoother (pitch);

    active = true;
    inTransition = false;
    anchorHops = 1000000;
    candCount = 0;
    noteAgeHops = 0;
    hopsSinceAttack = 0;
    unvoicedHops = 0;
    quietHops = 0;
    notePeakDb = levelDb;
    recentMaxDb = levelDb;
    dipArmed = false;

    // Controllers first, so the note starts at the right pitch and loudness.
    if (cfg.bendMode == BendMode::Quantized)
    {
        outPitch = (float) curNote;
    }
    else
    {
        outPitch = pitch;
        sendBend (outPitch - (float) curNote, offset, out);
    }
    updateExpression (levelDb, offset, out, ExprUpdate::Seed);

    MidiEvent e;
    e.sampleOffset = offset;
    e.size = 3;
    e.bytes[0] = (uint8_t) (0x90 | curChannel);
    e.bytes[1] = (uint8_t) curNote;
    e.bytes[2] = (uint8_t) noteOnVelocity (levelDb, false);
    out.push_back (e);
}

void NoteTracker::endNote (int offset, MidiEventList& out)
{
    if (! active)
        return;

    MidiEvent e;
    e.sampleOffset = offset;
    e.size = 3;
    e.bytes[0] = (uint8_t) (0x80 | curChannel);
    e.bytes[1] = (uint8_t) curNote;
    e.bytes[2] = 0;
    out.push_back (e);

    active = false;
    inTransition = false;
    candCount = 0;
    dipArmed = false;
    onsetGateDb = notePeakDb - kAfterNoteDropDb;
    idleMinDb = lastLevelDb;
    hopsSinceNoteEnd = 0;
    lastEndedPitch = lastPitch;
    deferHops = 0;
}

void NoteTracker::switchNote (int newNote, float levelDb, int offset, MidiEventList& out)
{
    const int oldNote = curNote;
    curNote = newNote;
    noteAgeHops = 0;
    notePeakDb = std::max (levelDb, notePeakDb - kLegatoPeakDropDb);

    if (cfg.bendMode == BendMode::Quantized)
        outPitch = (float) curNote;
    else
        sendBend (outPitch - (float) curNote, offset, out);

    updateExpression (levelDb, offset, out, ExprUpdate::Raise);

    // New note first, then release the old one: mono instruments in legato
    // mode slur, polyphonic instruments simply play the next note.
    MidiEvent on;
    on.sampleOffset = offset;
    on.size = 3;
    on.bytes[0] = (uint8_t) (0x90 | curChannel);
    on.bytes[1] = (uint8_t) curNote;
    on.bytes[2] = (uint8_t) noteOnVelocity (levelDb, true); // no new attack: a soft start
    out.push_back (on);

    MidiEvent off;
    off.sampleOffset = offset;
    off.size = 3;
    off.bytes[0] = (uint8_t) (0x80 | curChannel);
    off.bytes[1] = (uint8_t) oldNote;
    off.bytes[2] = 0;
    out.push_back (off);
}

//==============================================================================
void NoteTracker::processActive (const Input& in, int offset, MidiEventList& out)
{
    if (noteAgeHops < 1000000)
        ++noteAgeHops;
    if (hopsSinceAttack < 1000000)
        ++hopsSinceAttack;

    // --- has the string stopped sounding?
    notePeakDb  = std::max (notePeakDb - peakDecayPerHop, in.levelDb);
    recentMaxDb = std::max (recentMaxDb - recentDecayPerHop, in.levelDb);

    const bool quiet = in.levelDb < cfg.gateOffDb
                    || in.levelDb < notePeakDb - kRelativeDropDb;
    if (quiet)
    {
        if (++quietHops >= quietHopsNeeded)
        {
            endNote (offset, out);
            return;
        }
    }
    else
    {
        quietHops = 0;
    }

    // Far below its own level the note is only ringing out.
    const bool dying = in.levelDb < notePeakDb - kCollapseDb;

    // --- a new bow stroke on the same pitch: the level dips and comes back
    if (! dipArmed && recentMaxDb - in.levelDb >= kDipDb)
    {
        dipArmed = true;
        dipMinDb = in.levelDb;
    }
    if (dipArmed)
    {
        dipMinDb = std::min (dipMinDb, in.levelDb);
        // The stroke counts once the level is back near the note's own level.
        // A rise that stays far below it (a plucked string beating as it
        // fades) keeps waiting.
        if (in.levelDb >= dipMinDb + kRiseDb && ! dying)
        {
            dipArmed = false;
            if (noteAgeHops >= minRetriggerHops)
            {
                const float dipMin = dipMinDb;
                endNote (offset, out);
                idleMinDb = dipMin; // the rise out of the dip is the attack
                processIdle (in, offset, out);
                return;
            }
            recentMaxDb = in.levelDb; // ripple inside the attack, not a new stroke
        }
    }

    // --- pitch
    const bool usable = in.voiced
                     && in.clarity >= sustainClarity
                     && in.pitch >= (float) cfg.lowestNote - 1.5f
                     && in.pitch <= (float) cfg.highestNote + 1.5f;
    if (! usable)
    {
        if (++unvoicedHops > holdHops)
            endNote (offset, out);
        return;
    }

    unvoicedHops = 0;

    if (! inTransition)
    {
        const float reference = history[historyPos]; // oldest entry
        const bool jump = std::abs (in.pitch - lastPitch) > kJumpPerHop
                       || std::abs (in.pitch - reference) > kJumpWindow;
        if (! jump)
        {
            acceptPitch (in.pitch, in.levelDb, false, offset, out);
            return;
        }

        // Hold the output until the new pitch has settled. If a small jump
        // was accepted a moment ago, this one continues the same movement and
        // is measured from where that one began.
        inTransition = true;
        const int anchorLimit = (int) std::ceil (3.0f * periodInHops (in.pitch)) + 6;
        frozenPitch = anchorHops <= anchorLimit ? anchorPitch : reference;
        transitionPrev = in.pitch;
        transitionHops = 0;
        stableCount = 1;
        // A leap that begins soon after the note began (fresh or by a change of
        // note) may be a neighbouring string touched by accident (jump guard).
        jumpWatch = cfg.jumpGuard && noteAgeHops <= jumpWindowHops;
        return;
    }

    ++transitionHops;

    // A single stray estimate: back where we were, carry on as if nothing happened.
    if (std::abs (in.pitch - lastPitch) <= kJumpPerHop)
    {
        inTransition = false;
        acceptPitch (in.pitch, in.levelDb, false, offset, out);
        return;
    }

    if (std::abs (in.pitch - transitionPrev) <= kStableTolerance)
    {
        ++stableCount;
    }
    else
    {
        stableCount = 1;
        transitionPrev = in.pitch; // a new landing place
    }

    const float interval = std::abs (in.pitch - frozenPitch);
    const float periodHops = periodInHops (in.pitch);
    int needed = std::max (stableHops, (int) std::ceil (holdPeriods * periodHops) + 1);
    const bool octaveGuard = cfg.octaveSustainGuard;
    if (octaveGuard && isOctaveLike (interval))
        needed = std::max (needed, octaveStableHops);
    int timeout = std::max (transitionMaxHops, (int) std::ceil (kTransitionMaxPeriods * periodHops));
    // A longer octave time than the transition limit must not be cut short by
    // it. At the default 11 ms this changes nothing (transitionMaxHops is larger).
    if (octaveGuard && isOctaveLike (interval))
        timeout = std::max (timeout, octaveStableHops);
    // Jump guard: a wide leap away from the sounding note must last the lock
    // time before it is believed; the transition limit must not cut it short.
    // Until then nothing is sent and the note stays at its own pitch.
    if (jumpWatch && std::abs (in.pitch - lastPitch) >= (float) cfg.jumpIntervalSemis - kJumpIntervalSlack)
    {
        needed  = std::max (needed, jumpLockHops);
        timeout = std::max (timeout, jumpLockHops);
    }

    // An estimate one or more octaves down while the note's own period is
    // still clearly there is the detector slipping to a multiple of the
    // period (noise does that), not a new note. Only believed if it persists.
    if (octaveGuard && in.pitch < frozenPitch && isOctaveLike (interval)
        && in.hintClarity >= kSubharmonicHintClarity && transitionHops < subharmonicGuardHops)
        return;

    if (stableCount >= needed || transitionHops >= timeout)
    {
        const bool step = interval >= kNewNoteInterval;

        // A different pitch showing up while the note is dying away (an
        // overtone outlasting the fundamental, an open string, a neighbour
        // ringing) is not a slurred note. The note ends here; anything new has
        // to earn its own start.
        if (step && dying)
        {
            endNote (offset, out);
            return;
        }

        inTransition = false;
        fillHistory (in.pitch);
        if (step)
        {
            anchorHops = 1000000;
        }
        else
        {
            anchorPitch = frozenPitch;
            anchorHops = 0;
        }
        acceptPitch (in.pitch, in.levelDb, step, offset, out);
    }
}

void NoteTracker::acceptPitch (float pitch, float levelDb, bool step, int offset, MidiEventList& out)
{
    if (anchorHops < 1000000)
        ++anchorHops;
    lastPitch = pitch;
    pushHistory (pitch);
    if (step)
        resetSmoother (pitch);
    const float smoothed = smooth (pitch);

    if (cfg.bendMode == BendMode::Quantized)
    {
        outPitch = (float) curNote;
        if (std::abs (smoothed - (float) curNote) > 0.5f + kQuantizeHysteresis)
        {
            const int n = clampNote ((int) std::lround (smoothed));
            if (n != curNote)
                switchNote (n, levelDb, offset, out);
        }
        return;
    }

    outPitch = cfg.bendMode == BendMode::Continuous ? pitch : smoothed;

    // Ranges below a semitone clamp at the end of the bend; the next note
    // starts a little past the half-way point, so vibrato there cannot flutter.
    const float reach = std::max (cfg.bendRangeSemis, 0.5f + kQuantizeHysteresis);
    const bool outOfBendRange = std::abs (outPitch - (float) curNote) > reach;

    // While the bow attack is still building up, the string has not settled on
    // its pitch yet. A correction during that time is the same note finding
    // its place, so it is bent there instead of being struck again.
    const bool settling = step && ! outOfBendRange && attackInProgress();

    if ((step && ! settling) || outOfBendRange)
    {
        const int n = clampNote ((int) std::lround (outPitch));
        if (n != curNote)
        {
            switchNote (n, levelDb, offset, out);
            return;
        }
    }

    sendBend (outPitch - (float) curNote, offset, out);
}

//==============================================================================
void NoteTracker::sendBend (float semitones, int offset, MidiEventList& out)
{
    const float normalised = semitones / cfg.bendRangeSemis;
    const int value = clampi (kBendCentre + (int) std::lround (normalised * 8192.0f), 0, 16383);
    sendBendValue (value, offset, out);
}

void NoteTracker::sendBendValue (int value14, int offset, MidiEventList& out)
{
    if (value14 == lastBend14)
        return;

    MidiEvent e;
    e.sampleOffset = offset;
    e.size = 3;
    e.bytes[0] = (uint8_t) (0xE0 | (cfg.midiChannel - 1));
    e.bytes[1] = (uint8_t) (value14 & 0x7F);
    e.bytes[2] = (uint8_t) ((value14 >> 7) & 0x7F);
    out.push_back (e);
    lastBend14 = value14;
}

void NoteTracker::sendController (int number, int value, int offset, MidiEventList& out)
{
    MidiEvent e;
    e.sampleOffset = offset;
    e.size = 3;
    e.bytes[0] = (uint8_t) (0xB0 | (cfg.midiChannel - 1));
    e.bytes[1] = (uint8_t) clampi (number, 0, 127);
    e.bytes[2] = (uint8_t) clampi (value, 0, 127);
    out.push_back (e);
}

//==============================================================================
void NoteTracker::resetSmoother (float pitch) noexcept
{
    std::fill (smoothBuffer.begin(), smoothBuffer.end(), pitch);
    smoothSum = (double) pitch * (double) smoothLen;
    smoothPos = 0;
}

float NoteTracker::smooth (float pitch) noexcept
{
    const int n = smoothLen;
    smoothSum += (double) pitch - (double) smoothBuffer[(size_t) smoothPos];
    smoothBuffer[(size_t) smoothPos] = pitch;

    if (++smoothPos >= n)
    {
        smoothPos = 0;
        double exact = 0.0; // recompute once per turn so rounding cannot drift
        for (int i = 0; i < n; ++i)
            exact += (double) smoothBuffer[(size_t) i];
        smoothSum = exact;
    }

    const float mean = (float) (smoothSum / (double) n);
    return clampf (mean, pitch - kVibratoDeadband, pitch + kVibratoDeadband);
}

void NoteTracker::fillHistory (float pitch) noexcept
{
    for (float& h : history)
        h = pitch;
    historyPos = 0;
}

void NoteTracker::pushHistory (float pitch) noexcept
{
    history[historyPos] = pitch;
    historyPos = (historyPos + 1) % kHistory;
}

} // namespace vm
