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
    bendMode.store ((int) s.bendMode);
    dynamics.store ((int) s.dynamics);
    response.store ((int) s.response);
    bendRange.store (s.bendRangeSemis);
    vibratoSmoothing.store (s.vibratoSmoothing);
    enabled.store (s.enabled);
    keyFilter.store (s.keyFilter);
    keyRoot.store (s.keyRoot);
    scaleMode.store (s.scaleMode);
    pentatonic.store (s.pentatonic);
    pentaSustain.store (s.pentaSustain);
    triad.store (s.triad);
    triadChannel.store (s.triadChannel);
    triadWithRoot.store (s.triadWithRoot);
    lowestNote.store (s.lowestNote);
    highestNote.store (s.highestNote);
    fixedVelocity.store (s.fixedVelocity);
    midiChannel.store (s.midiChannel);
    inputGainDb.store (s.inputGainDb);
    gateDb.store (s.gateDb);
    a4Hz.store (s.a4Hz);
}

vm::Settings AudioHost::getSettings() const noexcept
{
    vm::Settings s;
    s.bendMode = (vm::BendMode) juce::jlimit (0, 2, bendMode.load());
    s.dynamics = (vm::DynamicsMode) juce::jlimit (0, 2, dynamics.load());
    s.response = (vm::Response) juce::jlimit (0, 2, response.load());
    s.bendRangeSemis = bendRange.load();
    s.vibratoSmoothing = vibratoSmoothing.load();
    s.enabled = enabled.load();
    s.keyFilter = keyFilter.load();
    s.keyRoot = keyRoot.load();
    s.scaleMode = scaleMode.load();
    s.pentatonic = pentatonic.load();
    s.pentaSustain = pentaSustain.load();
    s.triad = triad.load();
    s.triadChannel = triadChannel.load();
    s.triadWithRoot = triadWithRoot.load();
    s.lowestNote = lowestNote.load();
    s.highestNote = highestNote.load();
    s.fixedVelocity = fixedVelocity.load();
    s.midiChannel = midiChannel.load();
    s.inputGainDb = inputGainDb.load();
    s.gateDb = gateDb.load();
    s.a4Hz = a4Hz.load();
    return s;
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
    const int packed = mTriad.load();
    m.triadCount = juce::jlimit (0, 3, packed >> 24);
    for (int i = 0; i < 3; ++i)
        m.triad[i] = i < m.triadCount ? ((packed >> (8 * (2 - i))) & 0x7F) : -1;
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
            midiOut->sendMessageNow (juce::MidiMessage ((int) e.bytes[0], (int) e.bytes[1], (int) e.bytes[2]));

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
    const vm::Settings settings = getSettings();

    if (panic.exchange (false))
    {
        events.clear();
        engine.shutdown (events);

        const int mainChannel = juce::jlimit (1, 16, settings.midiChannel) - 1;
        const int secondChannel = juce::jlimit (1, 16, settings.triadChannel) - 1;
        for (int channel : { mainChannel, secondChannel })
        {
            if (channel == secondChannel && (channel == mainChannel || ! settings.triad))
                continue;

            vm::MidiEvent allNotesOff;
            allNotesOff.size = 3;
            allNotesOff.bytes[0] = (uint8_t) (0xB0 | channel);
            allNotesOff.bytes[1] = 123;
            allNotesOff.bytes[2] = 0;
            events.push_back (allNotesOff);
        }
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
    mTriad.store ((meter.triadCount << 24) | ((meter.triad[0] & 0x7F) << 16) | ((meter.triad[1] & 0x7F) << 8) | (meter.triad[2] & 0x7F));
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
    mTriad.store (0);
    mVoiced.store (false);
    mNote.store (-1);
    mLevelDb.store (-120.0f);
}

void AudioHost::audioDeviceError (const juce::String& errorMessage)
{
    const juce::ScopedLock lock (errorLock);
    errorText = errorMessage;
}
