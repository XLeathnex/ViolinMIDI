// ViolinMIDI core - second-order filter section (RBJ cookbook formulas).
#pragma once

#include <cmath>

namespace vm
{

class Biquad
{
public:
    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    void setLowpass (double sampleRate, double cutoffHz, double q) noexcept
    {
        const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - c) * 0.5 / a0;
        b1 = (1.0 - c) / a0;
        b2 = b0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighpass (double sampleRate, double cutoffHz, double q) noexcept
    {
        const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + c) * 0.5 / a0;
        b1 = -(1.0 + c) / a0;
        b2 = b0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - alpha) / a0;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        // Keep the state out of the denormal range during digital silence.
        if (std::abs (z1) < 1.0e-30) z1 = 0.0;
        if (std::abs (z2) < 1.0e-30) z2 = 0.0;
        return y;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace vm
