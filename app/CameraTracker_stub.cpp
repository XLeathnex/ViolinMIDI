// CameraTracker where there is no camera support (everything but macOS).
#include "CameraTracker.h"

struct CameraTracker::Native {};

CameraTracker::CameraTracker() : native (std::make_unique<Native>()) {}

CameraTracker::~CameraTracker()
{
    masterReference.clear();
}

juce::StringArray CameraTracker::availableCameras()   { return {}; }
bool CameraTracker::start (const juce::String&)       { return false; }
void CameraTracker::stop()                            {}
bool CameraTracker::isRunning() const                 { return false; }

juce::String CameraTracker::error() const
{
    return juce::String::fromUTF8 ("Die Kamera-Steuerung gibt es nur auf dem Mac.");
}

void CameraTracker::setTracking (const Tracking&)                {}
void CameraTracker::setBowCueSink (std::atomic<uint64_t>*) noexcept {}
