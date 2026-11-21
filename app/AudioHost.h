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
        int   triadCount = 0;        ///< notes sounding on the triad channel
        int   triad[3] = { -1, -1, -1 };
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
    vm::Settings getSettings() const noexcept;

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
    std::vector<float> silence;
    std::unique_ptr<juce::MidiOutput> midiOut;

    // settings: written by the window, read by the audio thread
    std::atomic<int>   bendMode { 0 }, dynamics { 0 }, response { 1 };
    std::atomic<int>   lowestNote { 34 }, highestNote { 105 };
    std::atomic<int>   fixedVelocity { 100 }, midiChannel { 1 };
    std::atomic<int>   keyRoot { 0 }, scaleMode { 0 }, triadChannel { 2 };
    std::atomic<float> inputGainDb { 0.0f }, gateDb { -45.0f }, a4Hz { 440.0f };
    std::atomic<float> bendRange { 2.0f }, vibratoSmoothing { 0.5f };
    std::atomic<bool>  enabled { true }, keyFilter { false }, pentatonic { false }, pentaSustain { false };
    std::atomic<bool>  triad { false }, triadWithRoot { true };
    std::atomic<bool>  panic { false };

    // meters: written by the audio thread, read by the window
    std::atomic<float> mLevelDb { -120.0f }, mInputPeak { 0.0f }, mClarity { 0.0f };
    std::atomic<float> mPitch { 0.0f }, mOutputPitch { 0.0f };
    std::atomic<int>   mNote { -1 }, mBlockSize { 0 };
    std::atomic<int>   mTriad { 0 };   // count and up to three notes, one byte each
    std::atomic<bool>  mVoiced { false }, mActive { false }, mHeld { false }, mRunning { false }, mHasInput { false };
    std::atomic<double> mSampleRate { 0.0 };
    std::atomic<unsigned int> mMidiMessages { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String errorText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioHost)
};
