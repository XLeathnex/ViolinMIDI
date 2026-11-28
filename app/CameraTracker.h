// The camera: frames in, body points out.
//
// On a Mac the frames come from AVFoundation and the body points from the
// Vision framework, which runs on the graphics chip or the Neural Engine. All
// of it happens on the camera's own queue; the audio thread never hears of it.
// Results arrive on the message thread, a few times per second more often than
// anything here needs. On other systems there is no camera support and start()
// says so.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CameraBow.h"
#include "CameraLogic.h"
#include "CameraMarker.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

class CameraTracker final
{
public:
    CameraTracker();
    ~CameraTracker();

    /** The cameras by name. Message thread. */
    static juce::StringArray availableCameras();

    /** Opens the camera with this name (empty: the first one) and starts
        looking. Asks for permission the first time; frames start once it is
        given. False with a reason in error() if it cannot work. */
    bool start (const juce::String& cameraName);
    void stop();
    bool isRunning() const;

    /** Why start() failed or frames stopped; empty if all is well. */
    juce::String error() const;

    /** Message thread, about 15 times per second: a small picture for the
        preview (already mirrored, as in a mirror) and the body points in the
        coordinates of that picture. */
    std::function<void (const juce::Image& preview, const cam::Pose& pose)> onFrame;

    /** Message thread: a start() that had to wait for the user's permission is
        decided. Either the camera runs now, or isRunning() is false and error()
        says why. Not called when stop() came first. */
    std::function<void()> onStateChanged;

    /** 2.1: what the camera queue looks for in each frame besides the body. The marker's
        position arrives in Pose::marker, the bow hand's state in Pose::bow (both through
        onFrame); the bow windows go straight to the audio thread (setBowCueSink). */
    struct Tracking
    {
        bool  marker = false;               ///< look for the colour marker (markerOn && colour.valid)
        cam::MarkerColour colour;
        float tolerance = 1.0f;
        bool  bow = false;                  ///< follow the bow hand; analyse every frame (up to 30 per second)
        cam::BowTracker::Settings bowSettings;
    };

    /** Message thread, any time. */
    void setTracking (const Tracking&);

    /** Message thread, before start(): where the bow windows go (AudioHost::bowCueSlot(), a
        cam::packCue word). It must outlive this tracker; stop() writes 0 there and nothing after. */
    void setBowCueSink (std::atomic<uint64_t>*) noexcept;

    struct Native;   // the platform's part

private:
    std::unique_ptr<Native> native;
    bool wanted = false;   // message thread: started and not stopped since
    int  startNumber = 0;  // message thread: counts the starts, so a late answer finds out it is out of date

    JUCE_DECLARE_WEAK_REFERENCEABLE (CameraTracker)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CameraTracker)
};
