// ViolinMIDI core - calibration: the passes over the takes, the reference notes,
// the metrics, the detection values, the dead zone and the orchestration of the
// analysis (see Calibration.h).
#include "Calibration.h"

namespace vm
{
namespace calib
{

SoundValues SoundValues::of (const Settings& s) noexcept
{
    SoundValues v;
    v.formant = s.formantCurve;
    v.hiCut = s.hiCutCurve;
    v.harmonyGainDb = s.harmonyGainDb;
    return v;
}

void SoundValues::applyTo (Settings& s) const noexcept
{
    s.formantCurve = formant;
    s.hiCutCurve = hiCut;
    s.harmonyGainDb = harmonyGainDb;
}

void copyCalibratedFields (const Settings& from, Settings& into) noexcept
{
    DetectionValues::of (from, 0.0f).applyTo (into);
    SoundValues::of (from).applyTo (into);
}

Settings neutralised (const Settings& before) noexcept
{
    return before;
}

Result Analyzer::run (const Inputs&, Control&, int)
{
    Result r;
    r.error = "not implemented";
    return r;
}

std::vector<MeasureRow> measurePass (const Take&, const Settings&)
{
    return {};
}

std::vector<RefNote> referenceNotes (const std::vector<MeasureRow>&, const Take&, const Instrument&, float)
{
    return {};
}

Metrics evaluate (const Take&, const std::vector<RefNote>&, const Settings&, double* cpuSec)
{
    if (cpuSec != nullptr)
        *cpuSec = 0.0;
    return {};
}

float pickSmallestMinimal (const std::vector<float>& candidates, const std::function<int (float)>&,
                           int, Limit& limit, int* evaluated)
{
    limit = Limit::none;
    if (evaluated != nullptr)
        *evaluated = 0;
    return candidates.empty() ? 0.0f : candidates.front();
}

float deadZoneFrom (const std::vector<CameraSample>&, const std::vector<Take>&, int& samples, float& p50, float& p95, Limit& limit)
{
    samples = 0;
    p50 = p95 = 0.0f;
    limit = Limit::none;
    return 0.06f;
}

} // namespace calib
} // namespace vm
