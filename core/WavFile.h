// ViolinMIDI core - WAV files for the calibration report (2.1): mono float32
// out; 16, 24 and 32-bit PCM and float32 in (the first channel). Header only,
// plain C++.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace vm
{
namespace wav
{

/** Mono, 32-bit float, at the device rate. False if the file could not be written. */
inline bool writeFloat32 (const std::string& /*path*/, const float* /*samples*/, size_t /*count*/, double /*sampleRate*/)
{
    return false;
}

/** The first channel as float, and the file's rate. False (and `error`) if unreadable. */
inline bool read (const std::string& /*path*/, std::vector<float>& samples, double& sampleRate, std::string* error = nullptr)
{
    samples.clear();
    sampleRate = 0.0;
    if (error != nullptr)
        *error = "not implemented";
    return false;
}

} // namespace wav
} // namespace vm
