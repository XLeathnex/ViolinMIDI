// Where the harmonizer's voices go (2.1): the output device and channel pair
// chosen in the harmonizer window, the clock hint when input and output are
// different devices, and what to do when devices come and go - JUCE falls back
// to the default input when a chosen output vanishes, and the violin must not
// be lost because BlackHole or a second interface is missing.
//
// Plain C++ without JUCE, so every decision is tested in violinmidi_tests.
// MainComponent gathers the facts from the AudioDeviceManager and applies the
// decision.
#pragma once

#include <string>
#include <vector>

namespace routing
{

/** Channel pairs of a device with this many output channels (an odd last channel is a mono pair). */
int pairCount (int outputChannels) noexcept;

/** "Out 1/2", "Out 3/4" ..., "Out 5 (mono)" for an odd last channel. */
std::string pairLabel (int pair, int outputChannels);

/** The pair limited to the device: 0 .. pairCount - 1 (0 without channels). */
int clampPair (int pair, int outputChannels) noexcept;

/** The output channels a pair enables: the first (0-based) and how many (2, 1 for mono, 0 if none). */
struct Channels { int first = 0, count = 0; };
Channels channelsFor (int pair, int outputChannels) noexcept;

/** Input and output are different devices (both named): their clocks drift apart. */
bool clocksDiffer (const std::string& inputDevice, const std::string& outputDevice);

/** At start: the output device the saved audio state names is present (otherwise it is removed
    from the state before JUCE opens it, so JUCE keeps the input). An empty name is kept. */
bool keepSavedOutput (const std::string& savedOutput, const std::vector<std::string>& outputs);

/** What the user wants. */
struct Wish
{
    std::string input;              ///< the input chosen in the settings window
    std::string output;             ///< "" = Automatisch (the macOS default while the harmonizer is on)
    int  pair = 0;
    bool harmonizerOn = false;
};

/** What is open now. */
struct InUse
{
    std::string input, output;      ///< "" = none
    Channels outputChannels;
};

/** What is there. */
struct Present
{
    std::vector<std::string> inputs, outputs;
    std::string defaultOutput;      ///< the macOS default output
    int wantedOutputChannels = 0;   ///< channels of Wish::output (or of the default with Automatisch)
};

/** After the device list changed: the setup to open, or nothing. */
struct Decision
{
    enum class Action
    {
        none,
        restoreInput,      ///< the wanted input is back or JUCE fell back: reopen it (with the output if present)
        openOutput,        ///< the wanted output is present but not in use, or on other channels
        dropOutput,        ///< the wanted output vanished: no output, the input stays
        openDefault        ///< Automatisch, harmonizer on, no output: the default device on the pair
    };

    Action action = Action::none;
    std::string input, output;      ///< the setup to apply ("" output = none)
    Channels outputChannels;
};

Decision reconcile (const Wish&, const InUse&, const Present&);

} // namespace routing
