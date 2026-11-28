// ViolinMIDI core - the Profimodus diagnosis folder of the calibration (2.1).
//
// Writes everything a later analysis needs to replay a calibration exactly:
// the raw takes as WAV, live and offline telemetry as CSV, the settings
// before and after (JSON and key=value), the session and report.json (schema
// "viologic-calibration/1"); and reads such a folder back (violinmidi_calibrate,
// the tests). Plain C++ with a small JSON writer of its own.
#pragma once

#include "Calibration.h"

#include <atomic>
#include <string>

namespace vm
{
namespace calib
{

/** What the app knows and the core does not. */
struct ExportInfo
{
    std::string createdAt, appVersion, os, cpu, device, outputDevice, profile, presetName;
    int  inputChannel = 1, bufferSize = 0, presetSlot = 0;
    bool presetHarmony = false;
    std::string settingsBeforeJson, settingsAfterJson;   ///< the preset format (fields::toVar), from the app
};

/** The whole folder (it is created). `progress` 0..1 if given. False with `error` on failure. */
bool writeDiagnosis (const std::string& folder, const Inputs&, const Result&, const ExportInfo&, std::string& error,
                     std::atomic<float>* progress = nullptr);

/** report.json alone, e.g. again with another decision ("pending", "saved", "reverted", "cancelled"). */
bool writeReportJson (const std::string& folder, const Inputs&, const Result&, const ExportInfo&, const char* decision);

/** A folder written by writeDiagnosis(), back into the analysis' inputs. */
bool readDiagnosis (const std::string& folder, Inputs& into, ExportInfo& info, std::string& error);

} // namespace calib
} // namespace vm
