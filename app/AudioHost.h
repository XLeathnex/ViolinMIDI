// Connects the audio interface to the engine and the engine to the virtual MIDI port.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine.h"

#include <atomic>
#include <memory>
#include <vector>

class AudioHost final : public juce::AudioIODeviceCallback
{
public:
    /** What the window shows; a snapshot taken from the audio thread. */
    struct Meters
    {
        float levelDb = -120.0f;     ///< level of the filtered signal, as the engine sees it
        float inputPeak = 0.0f;      ///< highest raw input sample since the last snapshot
        float clarity = 0.0f;
        float pitch = 0.0f;          ///< detected pitch, fractional MIDI note
        float outputPitch = 0.0f;    ///< pitch the instrument is asked to play
        int   note = -1;             ///< sounding MIDI note or -1
        bool  voiced = false;
        bool  active = false;
        bool  held = false;          ///< the note is kept by the pentatonic sustain
        int   triadCount = 0;        ///< notes sounding on the harmony channel
        int   triad[vm::scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        int   frozenNote = -1;       ///< the note kept by the solo freeze or -1
        bool  running = false;       ///< the audio device is delivering blocks
        bool  hasInput = false;      ///< the device has at least one active input channel
        double sampleRate = 0.0;
        int   blockSize = 0;
        unsigned int midiMessages = 0; ///< messages sent so far
    };

    AudioHost();
    ~AudioHost() override;

    /** Creates the virtual MIDI port other applications see. Message thread only. */
    bool openMidiPort (const juce::String& name);
    bool hasMidiPort() const noexcept { return midiOut != nullptr; }

    void setSettings (const vm::Settings& s) noexcept;

    /** Sends a controller on the main channel at the next audio block (channel 1
        with MPE). Per slot only the newest value waits: meant for a slow source
        such as the camera (slot 0: its X axis, slot 1: its Y axis), from the
        message thread. */
    static constexpr int kControllerSlots = 2;
    void sendController (int slot, int number, int value) noexcept
    {
        if (slot >= 0 && slot < kControllerSlots)
            pendingController[slot].store (((number & 0x7F) << 8) | (value & 0x7F));
    }

    /** Ends the sounding note and resets the controllers at the next audio block. */
    void requestAllNotesOff() noexcept { panic.store (true); }

    /** Takes a snapshot for the display and resets the input peak. */
    Meters takeMeters() noexcept;

    juce::String lastError() const;

    //==========================================================================
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

private:
    void send (const vm::MidiEventList& list) noexcept;

    vm::Engine engine;
    vm::MidiEventList events;
    std::vector<float> silence, outLeft, outRight;   // one chunk each
    std::unique_ptr<juce::MidiOutput> midiOut;

    // Settings: written by the window, read by the audio thread. The audio
    // thread only ever tries the lock and otherwise keeps what it has, so it
    // never waits; the window holds it for the length of one copy.
    juce::SpinLock settingsLock;
    vm::Settings sharedSettings;     // guarded by settingsLock
    vm::Settings audioSettings;      // the audio thread's own copy
    std::atomic<bool>  panic { false };
    std::atomic<int>   pendingController[kControllerSlots] { { -1 }, { -1 } };   // number << 8 | value, -1 = none

    // meters: written by the audio thread, read by the window
    std::atomic<float> mLevelDb { -120.0f }, mInputPeak { 0.0f }, mClarity { 0.0f };
    std::atomic<float> mPitch { 0.0f }, mOutputPitch { 0.0f };
    std::atomic<int>   mNote { -1 }, mBlockSize { 0 };
    std::atomic<int>   mTriadCount { 0 }, mFrozenNote { -1 };
    std::atomic<int>   mTriad[vm::scale::kMaxVoices];
    std::atomic<bool>  mVoiced { false }, mActive { false }, mHeld { false }, mRunning { false }, mHasInput { false };
    std::atomic<double> mSampleRate { 0.0 };
    std::atomic<unsigned int> mMidiMessages { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String errorText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioHost)
};
