// ViolinMIDI core - calibration: the German texts of the wizard and the report
// and the English notes for the developer (see Calibration.h).
#include "Calibration.h"

namespace vm
{
namespace calib
{

std::string stringName (int)
{
    return {};
}

std::string noteText (float, bool)
{
    return {};
}

std::string stepTitle (Step)
{
    return {};
}

std::string stepTarget (Step)
{
    return {};
}

std::string instruction (const TakeId&, const Instrument&)
{
    return {};
}

std::string hintText (const Progress&, const TakeId&, const Instrument&)
{
    return {};
}

std::string verdictName (Verdict)
{
    return {};
}

} // namespace calib
} // namespace vm
