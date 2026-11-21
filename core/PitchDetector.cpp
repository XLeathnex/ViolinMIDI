#include "PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined (__SSE2__) || defined (_M_X64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <emmintrin.h>
 #define VIOLINMIDI_SSE 1
#elif defined (__ARM_NEON) || defined (__aarch64__) || defined (_M_ARM64)
 #include <arm_neon.h>
 #define VIOLINMIDI_NEON 1
#endif

#ifndef VIOLINMIDI_SSE
 #define VIOLINMIDI_SSE 0
#endif
#ifndef VIOLINMIDI_NEON
 #define VIOLINMIDI_NEON 0
#endif

namespace vm
{

namespace
{
    // A peak this far below the best one is still accepted as "the same
    // periodicity". The shortest such lag is the fundamental period.
    constexpr float kOctaveRatio = 0.90f;
    // While a note sounds, its own period is kept as long as it stays this good.
    constexpr float kHintRatio = 0.80f;
    // ...and a shorter period only replaces it when it is practically perfect.
    constexpr float kStrictRatio = 0.985f;
    // A longer period that beats the chosen one by this margin raises doubt.
    constexpr float kDoubtRatio = 0.985f;
    // The hint matches candidates within about +-1 semitone.
    constexpr float kHintTolerance = 1.0595f;
    // Lobes this close above the chosen lag belong to the same period.
    constexpr float kNeighbourRatio = 1.07f;
    constexpr float kMinPeakValue = 0.25f;
    constexpr float kMinClarity = 0.35f;
    constexpr float kLargestWindowPeriods = 1.5f;
    constexpr float kLargestMinWindowMs = 6.0f;
    // Mean square below this (-90 dBFS) is treated as silence.
    constexpr double kSilenceMeanSquare = 1.0e-9;

    // Correlation of two sample runs. This loop is where the processing time
    // goes, so it uses the vector unit that every Mac has (SSE2 on Intel,
    // NEON on Apple Silicon).
    inline float dotProduct (const float* a, const float* b, int n) noexcept
    {
        int i = 0;
        float sum = 0.0f;

       #if VIOLINMIDI_SSE
        __m128 s0 = _mm_setzero_ps(), s1 = s0, s2 = s0, s3 = s0;
        for (; i + 16 <= n; i += 16)
        {
            s0 = _mm_add_ps (s0, _mm_mul_ps (_mm_loadu_ps (a + i),      _mm_loadu_ps (b + i)));
            s1 = _mm_add_ps (s1, _mm_mul_ps (_mm_loadu_ps (a + i + 4),  _mm_loadu_ps (b + i + 4)));
            s2 = _mm_add_ps (s2, _mm_mul_ps (_mm_loadu_ps (a + i + 8),  _mm_loadu_ps (b + i + 8)));
            s3 = _mm_add_ps (s3, _mm_mul_ps (_mm_loadu_ps (a + i + 12), _mm_loadu_ps (b + i + 12)));
        }
        for (; i + 4 <= n; i += 4)
            s0 = _mm_add_ps (s0, _mm_mul_ps (_mm_loadu_ps (a + i), _mm_loadu_ps (b + i)));
        float lanes[4];
        _mm_storeu_ps (lanes, _mm_add_ps (_mm_add_ps (s0, s1), _mm_add_ps (s2, s3)));
        sum = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
       #elif VIOLINMIDI_NEON
        float32x4_t s0 = vdupq_n_f32 (0.0f), s1 = s0, s2 = s0, s3 = s0;
        for (; i + 16 <= n; i += 16)
        {
            s0 = vaddq_f32 (s0, vmulq_f32 (vld1q_f32 (a + i),      vld1q_f32 (b + i)));
            s1 = vaddq_f32 (s1, vmulq_f32 (vld1q_f32 (a + i + 4),  vld1q_f32 (b + i + 4)));
            s2 = vaddq_f32 (s2, vmulq_f32 (vld1q_f32 (a + i + 8),  vld1q_f32 (b + i + 8)));
            s3 = vaddq_f32 (s3, vmulq_f32 (vld1q_f32 (a + i + 12), vld1q_f32 (b + i + 12)));
        }
        for (; i + 4 <= n; i += 4)
            s0 = vaddq_f32 (s0, vmulq_f32 (vld1q_f32 (a + i), vld1q_f32 (b + i)));
        float lanes[4];
        vst1q_f32 (lanes, vaddq_f32 (vaddq_f32 (s0, s1), vaddq_f32 (s2, s3)));
        sum = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
       #endif

        for (; i < n; ++i)
            sum += a[i] * b[i];
        return sum;
    }
}

void PitchDetector::prepare (double sampleRate, float absoluteMinHz, float absoluteMaxHz)
{
    fs = sampleRate;
    absTauHi = (int) std::ceil (fs / (double) absoluteMinHz) + 2;
    maxMinWindow = (int) std::ceil (fs * kLargestMinWindowMs * 0.001) + 1;

    const int largestWindow = std::max (maxMinWindow,
                                        (int) std::ceil (kLargestWindowPeriods * (float) (absTauHi + 1)) + 1);
    keep = absTauHi + 1 + largestWindow + 8;

    buffer.assign ((size_t) keep * 4, 0.0f);
    prefix.assign ((size_t) keep + 1, 0.0);
    nsdf.assign ((size_t) absTauHi + 4, 0.0f);
    candidates.clear();
    candidates.reserve ((size_t) absTauHi + 4);

    windowPeriods = 1.0f;
    minWindow = std::max (8, (int) std::lround (fs * 0.003));
    refineLimit = (int) std::lround (fs * 0.0045);
    setRange (absoluteMinHz, absoluteMaxHz);
    reset();
}

void PitchDetector::setRange (float minHz, float maxHz) noexcept
{
    minHz = std::max (minHz, 1.0f);
    maxHz = std::max (maxHz, minHz * 1.01f);
    int hi = (int) std::ceil (fs / (double) minHz);
    int lo = (int) std::floor (fs / (double) maxHz);
    hi = std::min (hi, absTauHi - 2);
    lo = std::max (lo, 2);
    if (lo >= hi)
        lo = std::max (2, hi - 1);
    tauLo = lo;
    tauHi = hi;
    updateNeed();
}

void PitchDetector::setWindow (float periods, float minWindowMs) noexcept
{
    windowPeriods = std::min (std::max (periods, 0.5f), kLargestWindowPeriods);
    minWindow = std::min (std::max (8, (int) std::lround (fs * (double) minWindowMs * 0.001)), maxMinWindow);
    updateNeed();
}

int PitchDetector::windowFor (int tau) const noexcept
{
    return std::max (minWindow, (int) std::ceil (windowPeriods * (float) tau));
}

void PitchDetector::updateNeed() noexcept
{
    need = std::min (keep, tauHi + 1 + windowFor (tauHi + 1));
}

void PitchDetector::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = keep; // the history starts out as silence
}

void PitchDetector::push (const float* samples, int count) noexcept
{
    const int capacity = (int) buffer.size();
    while (count > 0)
    {
        if (writePos == capacity)
        {
            std::memmove (buffer.data(), buffer.data() + (capacity - keep), (size_t) keep * sizeof (float));
            writePos = keep;
        }
        const int n = std::min (count, capacity - writePos);
        std::memcpy (buffer.data() + writePos, samples, (size_t) n * sizeof (float));
        writePos += n;
        samples += n;
        count -= n;
    }
}

double PitchDetector::meanSquare (int count) const noexcept
{
    count = std::min (std::max (count, 1), keep);
    const float* p = buffer.data() + (writePos - count);
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += (double) p[i] * (double) p[i];
    return sum / (double) count;
}

// Sharpens a short period by following its multiples: the peak near m periods
// is located just as precisely in samples, so the period itself becomes m times
// more precise. This is what keeps very high notes from jittering.
float PitchDetector::refine (float tau, float value) const noexcept
{
    const int limit = std::min (refineLimit, tauHi);
    double estimate = tau;

    for (int m = 2; (double) m * estimate <= (double) limit; ++m)
    {
        const int centre = (int) std::lround ((double) m * estimate);
        const int half = std::max (1, (int) std::floor (0.25 * estimate));
        const int lo = std::max (2, centre - half), hi = std::min (limit, centre + half);
        if (lo >= hi)
            break;

        int peak = lo;
        for (int t = lo + 1; t <= hi; ++t)
            if (nsdf[(size_t) t] > nsdf[(size_t) peak])
                peak = t;

        const float v0 = nsdf[(size_t) peak - 1], v1 = nsdf[(size_t) peak], v2 = nsdf[(size_t) peak + 1];
        if (v1 < v0 || v1 < v2 || v1 < 0.6f * value)
            break; // no clean peak here, keep what we have

        const float denom = v0 - 2.0f * v1 + v2;
        float offset = denom < -1.0e-9f ? 0.5f * (v0 - v2) / denom : 0.0f;
        offset = std::min (0.5f, std::max (-0.5f, offset));
        estimate = ((double) peak + (double) offset) / (double) m;
    }

    return (float) estimate;
}

PitchDetector::Result PitchDetector::analyse (float hintPeriod, bool preferHint) noexcept
{
    Result result;
    const int n = need;
    const float* y = buffer.data() + (writePos - n);

    double acc = 0.0;
    prefix[0] = 0.0;
    for (int i = 0; i < n; ++i)
    {
        acc += (double) y[i] * (double) y[i];
        prefix[(size_t) i + 1] = acc;
    }

    if (prefix[(size_t) n] - prefix[(size_t) (n - minWindow)] < kSilenceMeanSquare * (double) minWindow)
        return result;

    // Normalised square-difference function, 1 = perfectly periodic at this lag.
    // It is needed from lag 1 on, because the lobe around lag 0 must be skipped.
    nsdf[0] = 1.0f;
    for (int tau = 1; tau <= tauHi + 1; ++tau)
    {
        const int w = windowFor (tau);
        const float* a = y + (n - w);
        const float* b = a - tau;
        const double r = (double) dotProduct (a, b, w);
        const double m = (prefix[(size_t) n] - prefix[(size_t) (n - w)])
                       + (prefix[(size_t) (n - tau)] - prefix[(size_t) (n - tau - w)]);
        nsdf[(size_t) tau] = m > 1.0e-12 ? (float) (2.0 * r / m) : 0.0f;
    }

    // Key maxima: the highest point of every positive lobe after the function
    // has dipped below zero for the first time. The ripple that resonances of
    // bridge and pickup put on top of the lag-0 lobe is skipped this way, so a
    // bright low note cannot be mistaken for a high one.
    candidates.clear();
    float best = 0.0f;
    {
        int tau = 1;
        while (tau <= tauHi && nsdf[(size_t) tau] > 0.0f)
            ++tau;

        while (tau <= tauHi)
        {
            while (tau <= tauHi && nsdf[(size_t) tau] <= 0.0f)
                ++tau;
            if (tau > tauHi)
                break;

            int peak = tau;
            while (tau <= tauHi && nsdf[(size_t) tau] > 0.0f)
            {
                if (nsdf[(size_t) tau] > nsdf[(size_t) peak])
                    peak = tau;
                ++tau;
            }

            const float v0 = nsdf[(size_t) peak - 1], v1 = nsdf[(size_t) peak], v2 = nsdf[(size_t) peak + 1];
            const bool insideRange = peak >= tauLo && peak <= tauHi;
            const bool isMaximum = v1 >= v0 && v1 >= v2; // false when the lobe is cut off by the range limit
            if (insideRange && isMaximum && v1 > kMinPeakValue)
            {
                const float denom = v0 - 2.0f * v1 + v2;
                float offset = denom < -1.0e-9f ? 0.5f * (v0 - v2) / denom : 0.0f;
                offset = std::min (0.5f, std::max (-0.5f, offset));
                const float value = v1 - 0.25f * (v0 - v2) * offset;
                candidates.push_back ({ (float) peak + offset, value });
                best = std::max (best, value);
            }
        }
    }

    if (candidates.empty() || best < kMinClarity)
    {
        result.clarity = std::min (1.0f, std::max (0.0f, best));
        return result;
    }

    // The shortest lag that is nearly as periodic as the best one...
    const Candidate* chosen = nullptr;
    for (const auto& c : candidates)
    {
        if (c.value >= best * kOctaveRatio)
        {
            chosen = &c;
            break;
        }
    }

    // ...moved to the top of its group if a neighbouring lobe is higher.
    if (chosen != nullptr)
    {
        const float limit = chosen->tau * kNeighbourRatio;
        for (const Candidate* c = chosen + 1; c < candidates.data() + candidates.size() && c->tau <= limit; ++c)
            if (c->value > chosen->value)
                chosen = c;
    }

    // How periodic is the signal still at the period of the sounding note?
    if (hintPeriod > 0.0f)
    {
        const int lo = std::max (1, (int) std::floor (hintPeriod / 1.03f));
        const int hi = std::min (tauHi, (int) std::ceil (hintPeriod * 1.03f));
        float v = 0.0f;
        for (int t = lo; t <= hi; ++t)
            v = std::max (v, nsdf[(size_t) t]);
        result.hintClarity = std::min (1.0f, v);
    }

    // Continuity with the note that is already sounding.
    if (hintPeriod > 0.0f && preferHint)
    {
        const Candidate* nearHint = nullptr;
        for (const auto& c : candidates)
        {
            const float ratio = c.tau > hintPeriod ? c.tau / hintPeriod : hintPeriod / c.tau;
            if (ratio <= kHintTolerance && c.value >= best * kHintRatio
                && (nearHint == nullptr || c.value > nearHint->value))
                nearHint = &c;
        }

        if (nearHint != nullptr && chosen != nullptr && chosen != nearHint)
        {
            const bool chosenIsShorter = chosen->tau < nearHint->tau;
            if (! chosenIsShorter || chosen->value < best * kStrictRatio)
                chosen = nearHint;
        }
    }

    if (chosen == nullptr)
        return result;

    // Does a period two or three times as long fit clearly better? Then the
    // chosen one may be an overtone (a bowed string can start with a squeak).
    for (const auto& c : candidates)
    {
        const float ratio = c.tau / chosen->tau;
        const bool multiple = std::abs (ratio - 2.0f) < 0.06f || std::abs (ratio - 3.0f) < 0.09f;
        if (multiple && chosen->value < c.value * kDoubtRatio)
            result.octaveDoubt = true;
    }

    result.valid = true;
    result.period = refine (chosen->tau, chosen->value);
    result.clarity = std::min (1.0f, std::max (0.0f, chosen->value));
    return result;
}

} // namespace vm
