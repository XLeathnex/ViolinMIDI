// ViolinMIDI core - the Profimodus diagnosis folder (see CalibrationReport.h).
#include "CalibrationReport.h"

#include "WavFile.h"

namespace vm
{
namespace calib
{

bool writeDiagnosis (const std::string&, const Inputs&, const Result&, const ExportInfo&, std::string& error,
                     std::atomic<float>*)
{
    error = "not implemented";
    return false;
}

bool writeReportJson (const std::string&, const Inputs&, const Result&, const ExportInfo&, const char*)
{
    return false;
}

bool readDiagnosis (const std::string&, Inputs&, ExportInfo&, std::string& error)
{
    error = "not implemented";
    return false;
}

} // namespace calib
} // namespace vm
