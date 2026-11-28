// Offline tool: runs the calibration's analysis again on a Profimodus diagnosis
// folder and prints the changes and findings.
//
//   violinmidi_calibrate <folder> [--threads N] [--json out.json] [--professional]
//
// For checking a changed analysis against the violinist's real recordings.

#include "../core/Calibration.h"
#include "../core/CalibrationReport.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 2 || std::strcmp (argv[1], "--help") == 0)
    {
        std::printf ("usage: violinmidi_calibrate <folder> [--threads N] [--json out.json] [--professional]\n");
        return argc < 2 ? 2 : 0;
    }

    vm::calib::Inputs inputs;
    vm::calib::ExportInfo info;
    std::string error;
    if (! vm::calib::readDiagnosis (argv[1], inputs, info, error))
    {
        std::fprintf (stderr, "%s: %s\n", argv[1], error.c_str());
        return 1;
    }

    vm::calib::Analyzer::Control control;
    const auto result = vm::calib::Analyzer::run (inputs, control, 1);
    if (! result.ok)
    {
        std::fprintf (stderr, "analysis: %s\n", result.error.c_str());
        return 1;
    }
    return 0;
}
