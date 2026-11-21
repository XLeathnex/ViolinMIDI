#include "MidiRemote.h"

#include "Params.h"

#include <algorithm>

namespace
{
    void closeInput (std::unique_ptr<juce::MidiInput>& input)
    {
        if (input != nullptr)
        {
            input->stop();
            input.reset();
        }
    }

    bool isNumber (const juce::var& v)
    {
        return v.isInt() || v.isInt64() || v.isDouble();
    }

    // Clamped as double first: casting a huge double to int is undefined.
    int clampedInt (const juce::var& v, int low, int high, int fallback)
    {
        if (! isNumber (v))
            return fallback;
        return (int) juce::jlimit ((double) low, (double) high, (double) v);
    }

    MidiRemote::Binding sanitised (MidiRemote::Binding b)
    {
        using Type = MidiRemote::Binding::Type;
        if (b.type != Type::Controller && b.type != Type::Note)
            b.type = Type::Off;
        b.channel = juce::jlimit (0, 16, b.channel);
        b.number = juce::jlimit (0, 127, b.number);
        return b;
    }
}

MidiRemote::MidiRemote()
{
    bindings.resize (params::all().size());

    // Called on the message thread. The pedal may be plugged in after start or
    // unplugged mid-gig; the wanted name stays, so it reopens when it returns.
    deviceListConnection = juce::MidiDeviceListConnection::make ([this]
    {
        if (input != nullptr)
        {
            const auto id = input->getIdentifier();
            const auto devices = juce::MidiInput::getAvailableDevices();
            const bool present = std::any_of (devices.begin(), devices.end(),
                                              [&id] (const juce::MidiDeviceInfo& d) { return d.identifier == id; });
            if (! present)
                closeInput (input);
        }

        tryOpen();
        sendChangeMessage();
    });
}

MidiRemote::~MidiRemote()
{
    deviceListConnection.reset();
    closeInput (input);   // no more MIDI callbacks after this
    cancelPendingUpdate();
}

//==============================================================================
void MidiRemote::setExcludedName (const juce::String& ownOutputName)
{
    if (excluded == ownOutputName)
        return;

    excluded = ownOutputName;

    if (input != nullptr && excluded.isNotEmpty() && input->getName() == excluded)
        closeInput (input);

    sendChangeMessage();
}

juce::StringArray MidiRemote::availableInputs() const
{
    juce::StringArray names;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (device.name != excluded)
            names.addIfNotAlreadyThere (device.name);

    return names;
}

void MidiRemote::setInput (const juce::String& name)
{
    wantedInput = name;

    if (input != nullptr && input->getName() != name)
        closeInput (input);

    tryOpen();
    sendChangeMessage();
}

void MidiRemote::tryOpen()
{
    // Our own output fed back into us would be an endless loop.
    if (wantedInput.isEmpty() || wantedInput == excluded)
        return;

    if (input != nullptr)
    {
        if (input->getName() == wantedInput)
            return;
        closeInput (input);
    }

    // Matched by name: identifiers differ between machines and reboots.
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (device.name != wantedInput)
            continue;

        input = juce::MidiInput::openDevice (device.identifier, this);

        if (input != nullptr)
        {
            input->start();
            return;
        }
    }
}

//==============================================================================
MidiRemote::Binding MidiRemote::binding (int paramIndex) const
{
    if (! juce::isPositiveAndBelow (paramIndex, (int) bindings.size()))
        return {};
    return bindings[(size_t) paramIndex];
}

void MidiRemote::setBinding (int paramIndex, const Binding& newBinding)
{
    if (! juce::isPositiveAndBelow (paramIndex, (int) bindings.size()))
        return;

    const auto b = sanitised (newBinding);
    if (bindings[(size_t) paramIndex] == b)
        return;

    bindings[(size_t) paramIndex] = b;
    sendChangeMessage();
}

juce::String MidiRemote::toJson() const
{
    const auto& all = params::all();
    juce::Array<juce::var> list;

    for (size_t i = 0; i < bindings.size() && i < all.size(); ++i)
    {
        const auto& b = bindings[i];
        if (b.type == Binding::Type::Off)
            continue;

        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty ("id", all[i].id);
        entry->setProperty ("type", b.type == Binding::Type::Note ? "note" : "cc");
        entry->setProperty ("channel", b.channel);
        entry->setProperty ("number", b.number);
        list.add (juce::var (entry.get()));
    }

    return juce::JSON::toString (juce::var (list), true);
}

void MidiRemote::fromJson (const juce::String& json)
{
    for (auto& b : bindings)
        b = Binding();

    const auto parsed = juce::JSON::parse (json);
    const auto* list = parsed.getArray();
    if (list == nullptr)
        return;

    for (const auto& entry : *list)
    {
        if (! entry.isObject())
            continue;

        // Unknown ids come from newer or older versions; skipping them keeps the rest.
        const int index = params::indexOf (entry["id"].toString());
        if (! juce::isPositiveAndBelow (index, (int) bindings.size()))
            continue;

        Binding b;
        const auto type = entry["type"].toString();
        if (type == "cc")         b.type = Binding::Type::Controller;
        else if (type == "note")  b.type = Binding::Type::Note;
        else                      continue;

        if (! isNumber (entry["number"]))
            continue;

        b.channel = clampedInt (entry["channel"], 0, 16, 0);
        b.number = clampedInt (entry["number"], 0, 127, 0);
        bindings[(size_t) index] = b;
    }
}

//==============================================================================
void MidiRemote::startLearn (int paramIndex)
{
    if (! juce::isPositiveAndBelow (paramIndex, (int) bindings.size()))
        return;

    // A second click on the waiting row is the way out of learn.
    learning = (learning == paramIndex) ? -1 : paramIndex;
    sendChangeMessage();
}

void MidiRemote::cancelLearn()
{
    if (learning < 0)
        return;

    learning = -1;
    sendChangeMessage();
}

//==============================================================================
void MidiRemote::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // MIDI thread: no locks, no allocation, no device calls.
    if (! (message.isController() || message.isNoteOn()))   // isNoteOn() is false for velocity 0
        return;

    if (message.getRawDataSize() < 3)
        return;

    const auto* data = message.getRawData();

    int start1, size1, start2, size2;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 < 1)
        return;   // queue full: dropping beats blocking the MIDI thread

    auto& slot = queue[(size_t) start1];
    slot.status = data[0];
    slot.number = (uint8_t) (data[1] & 0x7f);
    slot.value  = (uint8_t) (data[2] & 0x7f);
    fifo.finishedWrite (1);

    triggerAsyncUpdate();
}

void MidiRemote::handleAsyncUpdate()
{
    for (;;)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 < 1)
            break;

        const auto m = queue[(size_t) start1];
        fifo.finishedRead (1);

        const bool isNote = (m.status & 0xf0) == 0x90;
        const int channel = (m.status & 0x0f) + 1;

        lastText = juce::String (isNote ? "Note " : "CC ") + juce::String ((int) m.number)
                 + ", Kanal " + juce::String (channel)
                 + (isNote ? ", Anschlag " : ", Wert ") + juce::String ((int) m.value);

        const auto type = isNote ? Binding::Type::Note : Binding::Type::Controller;

        if (juce::isPositiveAndBelow (learning, (int) bindings.size()))
        {
            // Only assigned, not applied: learning must never change a setting.
            auto& b = bindings[(size_t) learning];
            b.type = type;
            b.channel = channel;
            b.number = m.number;
            learning = -1;
            sendChangeMessage();
            continue;
        }

        // Several parameters may share one controller: all of them follow it.
        for (size_t i = 0; i < bindings.size(); ++i)
        {
            const auto& b = bindings[i];
            if (b.type == type && b.number == m.number && (b.channel == 0 || b.channel == channel) && onParam)
                onParam ((int) i, isNote, m.value);
        }
    }
}
