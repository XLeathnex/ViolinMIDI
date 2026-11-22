#include "AudioHost.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Blocks are processed in pieces of this size and the MIDI of each piece is
    // sent before the next one is analysed: a note found at the start of a large
    // block does not wait for the rest of it. It also bounds the event list, so
    // it never has to grow on the audio thread.
    constexpr int kChunk = 256;
    constexpr size_t kEventCapacity = 4096;
}

AudioHost::AudioHost()
{
    events.reserve (kEventCapacity);
    silence.assign ((size_t) kChunk, 0.0f);
    for (auto& note : mTriad)
        note.store (-1);
}

AudioHost::~AudioHost() = default;

bool AudioHost::openMidiPort (const juce::String& name)
{
    // A virtual source: MainStage, Logic and others list it like a keyboard.
    midiOut = juce::MidiOutput::createNewDevice (name);

    // One harmless message now, while no audio is running: the port sets up
    // its internal buffers here instead of on the audio thread later.
    if (midiOut != nullptr)
        midiOut->sendMessageNow (juce::MidiMessage::pitchWheel (1, 8192));

    return midiOut != nullptr;
}

void AudioHost::setSettings (const vm::Settings& s) noexcept
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    sharedSettings = s;
}

AudioHost::Meters AudioHost::takeMeters() noexcept
{
    Meters m;
    m.levelDb = mLevelDb.load();
    m.inputPeak = mInputPeak.exchange (0.0f);
    m.clarity = mClarity.load();
    m.pitch = mPitch.load();
    m.outputPitch = mOutputPitch.load();
    m.note = mNote.load();
    m.voiced = mVoiced.load();
    m.active = mActive.load();
    m.held = mHeld.load();
    m.triadCount = juce::jlimit (0, vm::scale::kMaxVoices, mTriadCount.load());
    for (int i = 0; i < vm::scale::kMaxVoices; ++i)
        m.triad[i] = i < m.triadCount ? mTriad[i].load() : -1;
    m.frozenNote = mFrozenNote.load();
    m.running = mRunning.load();
    m.hasInput = mHasInput.load();
    m.sampleRate = mSampleRate.load();
    m.blockSize = mBlockSize.load();
    m.midiMessages = mMidiMessages.load();
    return m;
}

juce::String AudioHost::lastError() const
{
    const juce::ScopedLock lock (errorLock);
    return errorText;
}

//==============================================================================
void AudioHost::send (const vm::MidiEventList& list) noexcept
{
    if (list.empty())
        return;

    if (midiOut != nullptr)
        for (const vm::MidiEvent& e : list)
            midiOut->sendMessageNow (e.size == 2 ? juce::MidiMessage ((int) e.bytes[0], (int) e.bytes[1])
                                                 : juce::MidiMessage ((int) e.bytes[0], (int) e.bytes[1], (int) e.bytes[2]));

    mMidiMessages.fetch_add ((unsigned int) list.size());
}

void AudioHost::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                  float* const* outputChannelData, int numOutputChannels,
                                                  int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    const juce::ScopedNoDenormals noDenormals;

    // This app only listens. Any output channels stay silent.
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    // The first active input channel carries the violin.
    const float* input = nullptr;
    for (int ch = 0; ch < numInputChannels && input == nullptr; ++ch)
        input = inputChannelData[ch];

    mHasInput.store (input != nullptr);
    {
        const juce::SpinLock::ScopedTryLockType lock (settingsLock);
        if (lock.isLocked())
            audioSettings = sharedSettings;
    }
    const vm::Settings& settings = audioSettings;

    if (panic.exchange (false))
    {
        events.clear();
        engine.shutdown (events);

        vm::Engine::appendAllNotesOff (settings, 0, events);
        send (events);
    }

    float peak = 0.0f;
    for (int offset = 0; offset < numSamples; offset += kChunk)
    {
        const int n = std::min (kChunk, numSamples - offset);
        const float* block = input != nullptr ? input + offset : silence.data();

        if (input != nullptr)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (block, n);
            peak = std::max (peak, std::max (std::abs (range.getStart()), std::abs (range.getEnd())));
        }

        events.clear();
        engine.process (block, n, settings, events);
        send (events);
    }

    if (peak > mInputPeak.load())
        mInputPeak.store (peak);

    const vm::Engine::Meter& meter = engine.meter();
    mLevelDb.store (meter.levelDb);
    mClarity.store (meter.clarity);
    mPitch.store (meter.pitch);
    mOutputPitch.store (meter.outputPitch);
    mNote.store (meter.note);
    mVoiced.store (meter.voiced);
    mActive.store (meter.active);
    mHeld.store (meter.held);
    for (int i = 0; i < vm::scale::kMaxVoices; ++i)
        mTriad[i].store (meter.triad[i]);
    mTriadCount.store (meter.triadCount);
    mFrozenNote.store (meter.frozenNote);
}

void AudioHost::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
    engine.prepare (sampleRate);
    events.clear();
    events.reserve (kEventCapacity);

    mSampleRate.store (sampleRate);
    mBlockSize.store (device != nullptr ? device->getCurrentBufferSizeSamples() : 0);
    mRunning.store (true);

    const juce::ScopedLock lock (errorLock);
    errorText.clear();
}

void AudioHost::audioDeviceStopped()
{
    // No more audio blocks arrive, so this is the only thread using the port now.
    events.clear();
    engine.shutdown (events);
    send (events);

    mRunning.store (false);
    mActive.store (false);
    mHeld.store (false);
    mTriadCount.store (0);
    mFrozenNote.store (-1);
    mVoiced.store (false);
    mNote.store (-1);
    mLevelDb.store (-120.0f);
}

void AudioHost::audioDeviceError (const juce::String& errorMessage)
{
    const juce::ScopedLock lock (errorLock);
    errorText = errorMessage;
}
