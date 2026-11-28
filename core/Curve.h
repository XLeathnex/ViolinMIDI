// ViolinMIDI core - a value that follows the played pitch ("key tracking"),
// given by a few points over the range of the violin.
//
// The audio harmonizer uses two of these: how much of the body resonance a
// voice keeps (formant curve) and where its low-pass sits (high-cut curve).
// Between the points the curve is a monotone cubic (Fritsch-Butland): smooth,
// and never above or below its neighbouring points. Below the first point and
// above the last it stays at their values.
//
// Plain data, fixed size: it lives inside Settings, which the audio thread
// copies, so nothing here may allocate.
#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace vm
{

struct Curve
{
    static constexpr int kMaxPoints = 12;

    int   count = 0;
    float x[kMaxPoints] = {};   ///< fractional MIDI note, ascending
    float y[kMaxPoints] = {};

    bool operator== (const Curve& o) const noexcept
    {
        if (count != o.count)
            return false;
        // written with < and > so strict warning levels (-Wfloat-equal) stay quiet
        for (int i = 0; i < count; ++i)
            if (x[i] < o.x[i] || x[i] > o.x[i] || y[i] < o.y[i] || y[i] > o.y[i])
                return false;
        return true;
    }

    bool operator!= (const Curve& o) const noexcept { return ! (*this == o); }
};

namespace curve
{
    /** The range the editors show: the four strings of a standard violin, G3 to E6. */
    constexpr float kLowNote  = 55.0f;
    constexpr float kHighNote = 88.0f;

    /** Points closer than this (in semitones) are one point. */
    constexpr float kMinSpacing = 0.25f;

    // The value ranges of the two curves the harmonizer uses.
    constexpr float kFormantLow = 0.0f, kFormantHigh = 1.0f;          ///< share of the body that is kept
    constexpr float kHiCutLowHz = 1000.0f, kHiCutHighHz = 16000.0f;   ///< cut-off of the voices' low-pass

    inline Curve make (std::initializer_list<std::pair<float, float>> points) noexcept
    {
        Curve c;
        for (const auto& p : points)
        {
            if (c.count >= Curve::kMaxPoints)
                break;
            c.x[c.count] = p.first;
            c.y[c.count] = p.second;
            ++c.count;
        }
        return c;
    }

    /** The same value everywhere. */
    inline Curve flat (float value) noexcept
    {
        return make ({ { kLowNote, value }, { kHighNote, value } });
    }

    /** Formant correction (the share of the body resonance the voices keep, 0..1).
        Low strings G3..A4: 5 % rising to 20 %, from A4 up a steady rise, E string
        from E5: 75 % rising to 95 %. */
    inline Curve defaultFormant() noexcept
    {
        return make ({ { 55.0f, 0.05f }, { 69.0f, 0.20f }, { 76.0f, 0.75f }, { 88.0f, 0.95f } });
    }

    /** High-cut of the voices in Hz. G and D string 8 to 6 kHz, from E5 (659 Hz)
        3.5 kHz falling to 2.5 kHz at the top. */
    inline Curve defaultHiCut() noexcept
    {
        return make ({ { 55.0f, 8000.0f }, { 69.0f, 6000.0f }, { 76.0f, 3500.0f }, { 88.0f, 2500.0f } });
    }

    /** Points inside the editor's range and the value range, sorted, none closer
        than kMinSpacing, at most kMaxPoints. A curve left with no point at all
        (or one that was never valid) becomes `fallback`. NaN counts as invalid. */
    inline Curve sanitised (const Curve& in, float yLow, float yHigh, const Curve& fallback) noexcept
    {
        Curve c;
        const int n = std::min (std::max (in.count, 0), Curve::kMaxPoints);

        for (int i = 0; i < n; ++i)
        {
            const float px = in.x[i], py = in.y[i];
            if (! std::isfinite (px) || ! std::isfinite (py))
                continue;
            c.x[c.count] = std::min (std::max (px, kLowNote), kHighNote);
            c.y[c.count] = std::min (std::max (py, yLow), yHigh);
            ++c.count;
        }

        // insertion sort by x: at most twelve points
        for (int i = 1; i < c.count; ++i)
            for (int j = i; j > 0 && c.x[j] < c.x[j - 1]; --j)
            {
                std::swap (c.x[j], c.x[j - 1]);
                std::swap (c.y[j], c.y[j - 1]);
            }

        // one point per place: of two that are too close the later one stays
        Curve out;
        for (int i = 0; i < c.count; ++i)
        {
            if (out.count > 0 && c.x[i] - out.x[out.count - 1] < kMinSpacing)
            {
                out.y[out.count - 1] = c.y[i];
                continue;
            }
            out.x[out.count] = c.x[i];
            out.y[out.count] = c.y[i];
            ++out.count;
        }

        return out.count > 0 ? out : fallback;
    }

    /** The curve's value at a played pitch. `logScale`: interpolated on a
        logarithmic scale (frequencies), else linearly. Real-time safe. */
    inline float value (const Curve& c, float note, bool logScale = false) noexcept
    {
        const int n = std::min (std::max (c.count, 0), Curve::kMaxPoints);
        if (n == 0)
            return 0.0f;

        const auto Y = [&c, logScale] (int i)
        {
            return logScale ? std::log2 (std::max (c.y[i], 1.0e-6f)) : c.y[i];
        };
        const auto out = [logScale] (float v) { return logScale ? std::exp2 (v) : v; };

        if (! std::isfinite (note) || n == 1 || note <= c.x[0])
            return out (Y (0));
        if (note >= c.x[n - 1])
            return out (Y (n - 1));

        int k = 0;
        while (k + 2 < n && note >= c.x[k + 1])
            ++k;

        // secant of segment i, 0 for a segment of no width
        const auto secant = [&c, &Y] (int i)
        {
            const float h = c.x[i + 1] - c.x[i];
            return h > 1.0e-6f ? (Y (i + 1) - Y (i)) / h : 0.0f;
        };

        // tangent at point i: one-sided at the ends, the weighted harmonic mean
        // of the two secants inside (zero where the curve turns), which keeps
        // every segment monotone.
        const auto tangent = [&] (int i)
        {
            if (i == 0)
                return secant (0);
            if (i == n - 1)
                return secant (n - 2);

            const float d0 = secant (i - 1), d1 = secant (i);
            if (d0 * d1 <= 0.0f)
                return 0.0f;

            const float h0 = c.x[i] - c.x[i - 1], h1 = c.x[i + 1] - c.x[i];
            return 3.0f * (h0 + h1) / ((2.0f * h1 + h0) / d0 + (h1 + 2.0f * h0) / d1);
        };

        const float h = c.x[k + 1] - c.x[k];
        if (! (h > 1.0e-6f))
            return out (Y (k + 1));

        const float t = (note - c.x[k]) / h, t2 = t * t, t3 = t2 * t;
        float m0 = tangent (k), m1 = tangent (k + 1);

        // The ends are one-sided: limit them so the end segments cannot overshoot either.
        const float d = secant (k);
        if (std::abs (d) < 1.0e-12f)
            m0 = m1 = 0.0f;
        else
        {
            m0 = std::min (std::max (m0 / d, 0.0f), 3.0f) * d;
            m1 = std::min (std::max (m1 / d, 0.0f), 3.0f) * d;
        }

        const float v = (2.0f * t3 - 3.0f * t2 + 1.0f) * Y (k) + (t3 - 2.0f * t2 + t) * h * m0
                      + (-2.0f * t3 + 3.0f * t2) * Y (k + 1) + (t3 - t2) * h * m1;

        // numerically it can still stray a hair outside the segment's ends
        const float lo = std::min (Y (k), Y (k + 1)), hi = std::max (Y (k), Y (k + 1));
        return out (std::min (std::max (v, lo), hi));
    }
}

} // namespace vm
