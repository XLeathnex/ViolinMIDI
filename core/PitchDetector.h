// ViolinMIDI core - monophonic period detector.
//
// Normalised square-difference function (McLeod) evaluated on the most recent
// samples, with a comparison window that grows with the lag. A candidate
// period T therefore only needs about two periods of signal: high violin
// notes are found within a few milliseconds, the low strings of a 7-string
// instrument take proportionally longer, and nothing waits for a fixed frame.
#pragma once

#include <vector>

namespace vm
{

class PitchDetector
{
public:
    struct Result
    {
        bool  valid   = false;
        float period  = 0.0f; ///< samples, fractional
        float clarity = 0.0f; ///< normalised correlation at the chosen period, 0..1
        float hintClarity = 0.0f; ///< how periodic the signal still is at the hinted period
        bool  octaveDoubt = false; ///< a period two or three times as long fits clearly better
    };

    /** Allocates for the widest range that can ever be requested. */
    void prepare (double sampleRate, float absoluteMinHz, float absoluteMaxHz);

    /** Narrows the search range. Never allocates. */
    void setRange (float minHz, float maxHz) noexcept;

    /** Comparison window = max (minWindowMs, periods * lag). Never allocates. */
    void setWindow (float periods, float minWindowMs) noexcept;

    void reset() noexcept;

    /** Appends samples to the history. */
    void push (const float* samples, int count) noexcept;

    /** Analyses the most recent samples.
        @param hintPeriod  period of the note currently sounding, or 0
        @param preferHint  keep the estimate at the hinted period while it is
                           still plausible, so it cannot flip between octaves */
    Result analyse (float hintPeriod, bool preferHint) noexcept;

    /** Mean square of the most recent `count` samples. */
    double meanSquare (int count) const noexcept;

    double sampleRate() const noexcept { return fs; }

private:
    struct Candidate { float tau; float value; };

    int  windowFor (int tau) const noexcept;
    float refine (float tau, float value) const noexcept;
    void updateNeed() noexcept;

    double fs = 48000.0;
    int tauLo = 2, tauHi = 2;         // current search range (inclusive)
    int absTauHi = 2;                 // allocation limit
    int need = 0;                     // history samples used by analyse()
    int keep = 0;                     // history samples preserved when the buffer wraps
    int writePos = 0;
    float windowPeriods = 1.0f;
    int minWindow = 1;
    int maxMinWindow = 1;
    int refineLimit = 1;              // longest lag used to sharpen short periods

    std::vector<float>  buffer;
    std::vector<double> prefix;       // running sum of squares
    std::vector<float>  nsdf;
    std::vector<Candidate> candidates;
};

} // namespace vm
