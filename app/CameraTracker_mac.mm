// The Mac's part of CameraTracker: AVFoundation delivers frames on its own
// queue, Vision finds the body points in them. Compiled with ARC (see
// app/CMakeLists.txt), so the Objective-C objects need no manual releasing.
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Vision/Vision.h>

#include "CameraTracker.h"

#include <atomic>

namespace
{
    constexpr double kSecondsBetweenAnalyses = 1.0 / 16.0;   // about 15 per second are plenty for a hand and a head
    constexpr int    kPreviewWidth = 320;

    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    NSArray<AVCaptureDevice*>* cameras()
    {
        // Deprecated in favour of discovery sessions, whose device types changed
        // name between system versions; this call lists every camera on all of them.
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return [AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo];
        #pragma clang diagnostic pop
    }

    juce::String nameOf (AVCaptureDevice* device)
    {
        return juce::String::fromUTF8 ([[device localizedName] UTF8String]);
    }

    /** Vision's points have the origin at the bottom left; ours at the top left,
        and the picture is shown mirrored. */
    void read (NSDictionary<VNHumanBodyPoseObservationJointName, VNRecognizedPoint*>* points,
               VNHumanBodyPoseObservationJointName name, cam::Point& out)
    {
        VNRecognizedPoint* p = points[name];
        if (p == nil)
            return;

        out.x = 1.0f - (float) p.location.x;
        out.y = 1.0f - (float) p.location.y;
        out.confidence = (float) p.confidence;
    }
}

//==============================================================================
@interface ArchetFrameReceiver : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
    CameraTracker::Native* native;
}
@end

struct CameraTracker::Native
{
    bool open (const juce::String& cameraName);
    void close();
    void handle (CMSampleBufferRef sample);

    void setError (const juce::String& message)
    {
        const juce::ScopedLock sl (lock);
        errorText = message;
    }

    juce::WeakReference<CameraTracker> owner;   // set once the tracker is fully constructed

    AVCaptureSession* session = nil;
    AVCaptureVideoDataOutput* output = nil;
    ArchetFrameReceiver* receiver = nil;
    dispatch_queue_t queue = nil;

    std::atomic<bool> running { false };
    double lastAnalysis = 0.0;          // camera queue only

    juce::CriticalSection lock;
    juce::String errorText;
};

@implementation ArchetFrameReceiver

- (void) captureOutput: (AVCaptureOutput*) captureOutput
 didOutputSampleBuffer: (CMSampleBufferRef) sampleBuffer
        fromConnection: (AVCaptureConnection*) connection
{
    juce::ignoreUnused (captureOutput, connection);

    if (native != nullptr)
        native->handle (sampleBuffer);
}

@end

//==============================================================================
bool CameraTracker::Native::open (const juce::String& cameraName)
{
    AVCaptureDevice* device = nil;
    for (AVCaptureDevice* candidate in cameras())
    {
        if (cameraName.isEmpty() || nameOf (candidate) == cameraName)
        {
            device = candidate;
            break;
        }
    }

    if (device == nil)
    {
        setError (cameraName.isEmpty() ? text ("Es ist keine Kamera angeschlossen.")
                                       : text ("Die Kamera \xe2\x80\x9e") + cameraName + text ("\xe2\x80\x9c ist nicht angeschlossen."));
        return false;
    }

    NSError* problem = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice: device error: &problem];
    AVCaptureSession* newSession = [[AVCaptureSession alloc] init];

    if (input == nil || ! [newSession canAddInput: input])
    {
        setError (text ("Die Kamera l\xc3\xa4sst sich nicht \xc3\xb6" "ffnen. Vielleicht benutzt sie gerade ein anderes Programm."));
        return false;
    }

    // A small picture is all the body model needs, and it keeps the load low.
    if ([newSession canSetSessionPreset: AVCaptureSessionPreset640x480])
        newSession.sessionPreset = AVCaptureSessionPreset640x480;

    [newSession addInput: input];

    AVCaptureVideoDataOutput* newOutput = [[AVCaptureVideoDataOutput alloc] init];
    newOutput.videoSettings = @{ (NSString*) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
    newOutput.alwaysDiscardsLateVideoFrames = YES;

    if (! [newSession canAddOutput: newOutput])
    {
        setError (text ("Die Kamera liefert kein Bild in einem brauchbaren Format."));
        return false;
    }

    receiver = [[ArchetFrameReceiver alloc] init];
    receiver->native = this;
    queue = dispatch_queue_create ("app.violinmidi.camera", DISPATCH_QUEUE_SERIAL);
    [newOutput setSampleBufferDelegate: receiver queue: queue];
    [newSession addOutput: newOutput];

    session = newSession;
    output = newOutput;
    lastAnalysis = 0.0;
    setError ({});
    running = true;

    // Starting takes a moment; it must not hold up the window.
    dispatch_async (queue, ^{ [newSession startRunning]; });
    return true;
}

void CameraTracker::Native::close()
{
    running = false;

    if (output != nil)
        [output setSampleBufferDelegate: nil queue: nil];

    if (queue != nil && receiver != nil)
    {
        // Whatever frame is being looked at right now is finished first.
        ArchetFrameReceiver* r = receiver;
        AVCaptureSession* s = session;
        dispatch_sync (queue, ^{
            r->native = nullptr;
            if (s != nil && [s isRunning])
                [s stopRunning];
        });
    }

    session = nil;
    output = nil;
    receiver = nil;
    queue = nil;
}

void CameraTracker::Native::handle (CMSampleBufferRef sample)
{
    if (! running.load())
        return;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastAnalysis < kSecondsBetweenAnalyses)
        return;
    lastAnalysis = now;

    CVPixelBufferRef pixels = CMSampleBufferGetImageBuffer (sample);
    if (pixels == nullptr)
        return;

    cam::Pose pose;
    pose.timeSec = now;

    @autoreleasepool
    {
        VNDetectHumanBodyPoseRequest* request = [[VNDetectHumanBodyPoseRequest alloc] init];
        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCVPixelBuffer: pixels options: @{}];
        NSError* problem = nil;

        if ([handler performRequests: @[request] error: &problem])
        {
            VNHumanBodyPoseObservation* body = (VNHumanBodyPoseObservation*) [request.results firstObject];
            NSDictionary<VNHumanBodyPoseObservationJointName, VNRecognizedPoint*>* points =
                body != nil ? [body recognizedPointsForJointsGroupName: VNHumanBodyPoseObservationJointsGroupNameAll error: &problem]
                            : nil;

            if (points != nil)
            {
                pose.valid = true;
                read (points, VNHumanBodyPoseObservationJointNameNose, pose.nose);
                read (points, VNHumanBodyPoseObservationJointNameLeftEye, pose.leftEye);
                read (points, VNHumanBodyPoseObservationJointNameRightEye, pose.rightEye);
                read (points, VNHumanBodyPoseObservationJointNameLeftEar, pose.leftEar);
                read (points, VNHumanBodyPoseObservationJointNameRightEar, pose.rightEar);
                read (points, VNHumanBodyPoseObservationJointNameNeck, pose.neck);
                read (points, VNHumanBodyPoseObservationJointNameLeftShoulder, pose.leftShoulder);
                read (points, VNHumanBodyPoseObservationJointNameRightShoulder, pose.rightShoulder);
                read (points, VNHumanBodyPoseObservationJointNameLeftElbow, pose.leftElbow);
                read (points, VNHumanBodyPoseObservationJointNameRightElbow, pose.rightElbow);
                read (points, VNHumanBodyPoseObservationJointNameLeftWrist, pose.leftWrist);
                read (points, VNHumanBodyPoseObservationJointNameRightWrist, pose.rightWrist);
            }
        }
    }

    // The small mirrored picture for the window.
    juce::Image preview;
    if (CVPixelBufferGetPixelFormatType (pixels) == kCVPixelFormatType_32BGRA
        && CVPixelBufferLockBaseAddress (pixels, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess)
    {
        const auto* base = (const juce::uint8*) CVPixelBufferGetBaseAddress (pixels);
        const int width = (int) CVPixelBufferGetWidth (pixels), height = (int) CVPixelBufferGetHeight (pixels);
        const size_t stride = CVPixelBufferGetBytesPerRow (pixels);

        if (base != nullptr && width > 0 && height > 0)
        {
            const int step = juce::jmax (1, width / kPreviewWidth);
            const int w = width / step, h = height / step;
            preview = juce::Image (juce::Image::RGB, w, h, false);
            juce::Image::BitmapData dest (preview, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < h; ++y)
            {
                const juce::uint8* row = base + (size_t) (y * step) * stride;
                for (int x = 0; x < w; ++x)
                {
                    const juce::uint8* p = row + (size_t) (width - 1 - x * step) * 4;   // blue, green, red, alpha
                    dest.setPixelColour (x, y, juce::Colour (p[2], p[1], p[0]));
                }
            }
        }

        CVPixelBufferUnlockBaseAddress (pixels, kCVPixelBufferLock_ReadOnly);
    }

    juce::MessageManager::callAsync ([weak = owner, preview, pose]
    {
        if (auto* tracker = weak.get())
            if (tracker->onFrame != nullptr)
                tracker->onFrame (preview, pose);
    });
}

//==============================================================================
CameraTracker::CameraTracker() : native (std::make_unique<Native>())
{
    native->owner = this;
}

CameraTracker::~CameraTracker()
{
    stop();
    masterReference.clear();
}

juce::StringArray CameraTracker::availableCameras()
{
    juce::StringArray names;
    for (AVCaptureDevice* device in cameras())
        names.add (nameOf (device));
    return names;
}

bool CameraTracker::start (const juce::String& cameraName)
{
    stop();

    const AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeVideo];

    if (status == AVAuthorizationStatusDenied || status == AVAuthorizationStatusRestricted)
    {
        native->setError (text ("Der Zugriff auf die Kamera ist nicht erlaubt. In den Systemeinstellungen unter "
                                "\xe2\x80\x9e" "Datenschutz & Sicherheit\xe2\x80\x9c \xe2\x86\x92 \xe2\x80\x9eKamera\xe2\x80\x9c f\xc3\xbcr VioLogic einschalten."));
        return false;
    }

    if (status == AVAuthorizationStatusNotDetermined)
    {
        // macOS asks now. Frames follow once the question has been answered.
        const juce::WeakReference<CameraTracker> weak (this);
        const juce::String name (cameraName);

        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeVideo completionHandler: ^(BOOL granted)
        {
            const bool allowed = granted == YES;
            juce::MessageManager::callAsync ([weak, name, allowed]
            {
                if (auto* tracker = weak.get())
                {
                    if (allowed)
                        tracker->start (name);
                    else
                        tracker->native->setError (text ("Ohne Zugriff auf die Kamera gibt es keine Kamera-Steuerung."));
                }
            });
        }];

        native->setError (text ("Warte auf die Erlaubnis f\xc3\xbcr die Kamera \xe2\x80\xa6"));
        return true;
    }

    return native->open (cameraName);
}

void CameraTracker::stop()
{
    native->close();
}

bool CameraTracker::isRunning() const
{
    return native->running.load();
}

juce::String CameraTracker::error() const
{
    const juce::ScopedLock sl (native->lock);
    return native->errorText;
}
