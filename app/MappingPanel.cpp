#include "MappingPanel.h"

#include "Brand.h"
#include "Params.h"

namespace
{
    constexpr int kMargin = 16;
    constexpr int kRowHeight = 28, kRowGap = 4;
    constexpr int kColumnGap = 8;
    constexpr int kTypeWidth = 104, kChannelWidth = 76, kNumberWidth = 56, kLearnWidth = 84;

    // Where Theme::positionComboBoxText puts a combo's text (label at x 6, plus
    // the label's own 5 px border); headers start there to line up with it.
    constexpr int kComboTextInset = 11;

    // Same corner as the combo boxes drawn by Theme.
    constexpr float kRadius = 5.0f;

    const juce::Colour kDim = brand::colour::secondary;

    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    struct Columns
    {
        juce::Rectangle<int> name, type, channel, number, learn;
    };

    // One function for header and rows, so they cannot drift apart.
    Columns columnsFor (juce::Rectangle<int> r)
    {
        Columns c;
        c.learn = r.removeFromRight (kLearnWidth);
        r.removeFromRight (kColumnGap);
        c.number = r.removeFromRight (kNumberWidth);
        r.removeFromRight (kColumnGap);
        c.channel = r.removeFromRight (kChannelWidth);
        r.removeFromRight (kColumnGap);
        c.type = r.removeFromRight (kTypeWidth);
        r.removeFromRight (kColumnGap);
        c.name = r;
        return c;
    }
}

//==============================================================================
class MappingPanel::Row final : public juce::Component
{
public:
    Row (MidiRemote& r, int paramIndex, const juce::String& name)
        : remote (r), index (paramIndex)
    {
        nameLabel.setText (name, juce::dontSendNotification);
        nameLabel.setFont (brand::font (13.0f));
        nameLabel.setBorderSize ({ 1, 0, 1, 0 });   // text starts exactly under the header
        nameLabel.setJustificationType (juce::Justification::centredLeft);
        nameLabel.setMinimumHorizontalScale (0.8f);
        addAndMakeVisible (nameLabel);

        typeBox.addItem ("Aus", 1);
        typeBox.addItem ("Controller", 2);
        typeBox.addItem ("Note", 3);
        typeBox.onChange = [this] { commit(); };
        typeBox.setTooltip (text ("Womit dieser Regler gesteuert wird: ein Controller (Fu\xc3\x9f" "pedal, Fader) oder eine Note (Taster)."));
        addAndMakeVisible (typeBox);

        channelBox.addItem ("Alle", 1);
        for (int channel = 1; channel <= 16; ++channel)
            channelBox.addItem (juce::String (channel), channel + 1);
        channelBox.onChange = [this] { commit(); };
        channelBox.setTooltip (text ("Auf welchem MIDI-Kanal die Nachricht kommen muss. \xe2\x80\x9e" "Alle\xe2\x80\x9c nimmt jeden Kanal."));
        addAndMakeVisible (channelBox);

        // The field is drawn by paint() with the combo corners; the editor stays see-through.
        numberLabel.setEditable (true, false, false);
        numberLabel.setFont (brand::font (13.0f));
        numberLabel.setJustificationType (juce::Justification::centred);
        numberLabel.setColour (juce::Label::textWhenEditingColourId, brand::colour::text);
        numberLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
        numberLabel.setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
        numberLabel.onEditorShow = [this]
        {
            if (auto* editor = numberLabel.getCurrentTextEditor())
            {
                editor->setJustification (juce::Justification::centred);
                editor->setInputRestrictions (3, "0123456789");
            }
        };
        numberLabel.onTextChange = [this]
        {
            const int number = juce::jlimit (0, 127, numberLabel.getText().getIntValue());
            numberLabel.setText (juce::String (number), juce::dontSendNotification);
            commit();
        };
        numberLabel.setTooltip (text ("Nummer des Controllers oder der Note, 0 bis 127. Doppelklick zum Eintippen."));
        addAndMakeVisible (numberLabel);

        learnButton.setTooltip (text ("\xc3\x9c" "bernimmt die n\xc3\xa4" "chste eintreffende Nachricht (Controller oder Note). Noch einmal klicken bricht ab."));
        learnButton.onClick = [this] { remote.startLearn (index); };
        addAndMakeVisible (learnButton);

        refresh();
    }

    /** Takes the binding from the remote without notifications, so nothing is sent back. */
    void refresh()
    {
        const auto b = remote.binding (index);

        typeBox.setSelectedId ((int) b.type + 1, juce::dontSendNotification);
        channelBox.setSelectedId (b.channel + 1, juce::dontSendNotification);

        // setText would close an open editor and throw away what is being typed.
        if (! numberLabel.isBeingEdited())
            numberLabel.setText (juce::String (b.number), juce::dontSendNotification);

        const bool active = b.type != MidiRemote::Binding::Type::Off;
        channelBox.setEnabled (active);
        numberLabel.setEnabled (active);

        const bool learning = remote.learningIndex() == index;
        learnButton.setButtonText (learning ? text ("Warte \xe2\x80\xa6") : juce::String ("Lernen"));
        learnButton.setToggleState (learning, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (brand::colour::control);
        g.fillRoundedRectangle (numberLabel.getBounds().toFloat(), kRadius);
    }

    void resized() override
    {
        const auto c = columnsFor (getLocalBounds());
        nameLabel.setBounds (c.name);
        typeBox.setBounds (c.type);
        channelBox.setBounds (c.channel);
        numberLabel.setBounds (c.number);
        learnButton.setBounds (c.learn);
    }

private:
    void commit()
    {
        MidiRemote::Binding b;
        b.type = static_cast<MidiRemote::Binding::Type> (juce::jlimit (0, 2, typeBox.getSelectedId() - 1));
        b.channel = juce::jlimit (0, 16, channelBox.getSelectedId() - 1);
        b.number = juce::jlimit (0, 127, numberLabel.getText().getIntValue());

        // Immediately, not only after the change message comes back.
        const bool active = b.type != MidiRemote::Binding::Type::Off;
        channelBox.setEnabled (active);
        numberLabel.setEnabled (active);

        remote.setBinding (index, b);
    }

    MidiRemote& remote;
    const int index;

    juce::Label nameLabel, numberLabel;
    juce::ComboBox typeBox, channelBox;
    juce::TextButton learnButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Row)
};

//==============================================================================
MappingPanel::MappingPanel (MidiRemote& midiRemote)
    : remote (midiRemote)
{
    inputLabel.setText ("MIDI-Eingang", juce::dontSendNotification);
    inputLabel.setFont (brand::font (13.0f));
    inputLabel.setColour (juce::Label::textColourId, kDim);
    inputLabel.setBorderSize ({ 1, 0, 1, 0 });
    inputLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (inputLabel);

    inputBox.onChange = [this]
    {
        const int index = inputBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, inputNames.size()) && inputNames[index] != remote.inputName())
            remote.setInput (inputNames[index]);
    };
    inputBox.setTooltip (text ("Der MIDI-Eingang f\xc3\xbcr Fu\xc3\x9f" "pedal oder Controller. Ein abgezogenes Ger\xc3\xa4t bleibt gew\xc3\xa4hlt und wird wieder ge\xc3\xb6" "ffnet, sobald es da ist."));
    addAndMakeVisible (inputBox);

    lastLabel.setFont (brand::font (13.0f));
    lastLabel.setColour (juce::Label::textColourId, kDim);
    lastLabel.setBorderSize ({ 1, 0, 1, 0 });
    lastLabel.setJustificationType (juce::Justification::centredLeft);
    lastLabel.setMinimumHorizontalScale (0.8f);
    lastLabel.setTooltip (text ("Die zuletzt empfangene Nachricht: zum Pr\xc3\xbc" "fen, ob das Pedal ankommt und was es sendet."));
    addAndMakeVisible (lastLabel);

    const auto presetTip = text ("Ein Program Change auf diesem Kanal l\xc3\xa4" "dt das Preset mit dieser Nummer: "
                                 "Program 0 l\xc3\xa4" "dt Preset 001, Program 127 Preset 128.");

    programLabel.setText ("Presets per Program Change", juce::dontSendNotification);
    programLabel.setFont (brand::font (13.0f));
    programLabel.setColour (juce::Label::textColourId, kDim);
    programLabel.setBorderSize ({ 1, 0, 1, 0 });
    programLabel.setJustificationType (juce::Justification::centredLeft);
    programLabel.setMinimumHorizontalScale (0.8f);
    programLabel.setTooltip (presetTip);
    addAndMakeVisible (programLabel);

    programBox.addItem ("Aus", 1);
    programBox.addItem (text ("Alle Kan\xc3\xa4le"), 2);
    for (int channel = 1; channel <= 16; ++channel)
        programBox.addItem ("Kanal " + juce::String (channel), channel + 2);
    programBox.setTooltip (presetTip);
    programBox.onChange = [this] { remote.setProgramChannel (programBox.getSelectedId() - 2); };
    addAndMakeVisible (programBox);

    const auto& all = params::all();
    for (size_t i = 0; i < all.size(); ++i)
    {
        rows.push_back (std::make_unique<Row> (remote, (int) i, all[i].name));
        content.addAndMakeVisible (*rows.back());
    }

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    remote.addChangeListener (this);
    rebuildInputList();
    refreshProgramBox();
    refreshLastMessage();
    startTimerHz (5);
}

MappingPanel::~MappingPanel()
{
    remote.removeChangeListener (this);

    // A learn left waiting after the window closes would silently remap the next pedal press.
    remote.cancelLearn();
}

void MappingPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildInputList();
    refreshProgramBox();
    refreshRows();
}

void MappingPanel::timerCallback()
{
    refreshLastMessage();
}

//==============================================================================
void MappingPanel::rebuildInputList()
{
    const auto ports = remote.availableInputs();
    const auto wanted = remote.inputName();

    // An unplugged pedal stays chosen, so the choice survives until it is back.
    const bool missing = wanted.isNotEmpty() && ! ports.contains (wanted);

    juce::StringArray names;
    names.add (juce::String());
    names.addArray (ports);
    if (missing)
        names.add (wanted);

    // Only refill when the list really changed: clearing closes an open popup.
    if (names != inputNames || missing != listedMissing)
    {
        inputNames = names;
        listedMissing = missing;

        inputBox.clear (juce::dontSendNotification);
        inputBox.addItem ("Kein Eingang", 1);

        for (int i = 1; i < names.size(); ++i)
        {
            const bool notConnected = missing && i == names.size() - 1;
            inputBox.addItem (notConnected ? names[i] + " (nicht verbunden)" : names[i], i + 1);
        }
    }

    inputBox.setSelectedId (juce::jmax (0, inputNames.indexOf (wanted)) + 1, juce::dontSendNotification);
}

void MappingPanel::refreshRows()
{
    for (auto& row : rows)
        row->refresh();
}

void MappingPanel::refreshProgramBox()
{
    programBox.setSelectedId (remote.programChannel() + 2, juce::dontSendNotification);
}

void MappingPanel::refreshLastMessage()
{
    const auto last = remote.lastMessageText();
    const auto shown = last.isEmpty() ? juce::String ("Noch nichts empfangen.")
                                      : "Zuletzt empfangen: " + last;

    if (lastLabel.getText() != shown)
        lastLabel.setText (shown, juce::dontSendNotification);
}

//==============================================================================
void MappingPanel::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (brand::colour::separator);
    g.fillRect (kMargin, separatorY, getWidth() - 2 * kMargin, 1);

    const auto c = columnsFor (headerArea);
    g.setColour (kDim);
    g.setFont (brand::font (12.0f));
    g.drawText ("Parameter", c.name, juce::Justification::centredLeft);
    g.drawText ("Typ", c.type.withTrimmedLeft (kComboTextInset), juce::Justification::centredLeft);
    g.drawText ("Kanal", c.channel.withTrimmedLeft (kComboTextInset), juce::Justification::centredLeft);
    g.drawText ("Nummer", c.number, juce::Justification::centred);
}

void MappingPanel::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 0);
    area.removeFromTop (kMargin);

    // The port menu starts at the type column, so it lines up with the table below.
    auto inputRow = area.removeFromTop (kRowHeight);
    const auto inputColumns = columnsFor (inputRow);
    inputLabel.setBounds (inputColumns.name);
    inputBox.setBounds (inputRow.withLeft (inputColumns.type.getX()));

    area.removeFromTop (6);
    lastLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (12);

    // Same label column and control width as the port row.
    auto programRow = area.removeFromTop (kRowHeight);
    const auto programColumns = columnsFor (programRow);
    programLabel.setBounds (programColumns.name);
    programBox.setBounds (programRow.withLeft (programColumns.type.getX()));
    area.removeFromTop (12);

    separatorY = area.getY();
    area.removeFromTop (1 + 10);
    headerArea = area.removeFromTop (16);
    area.removeFromTop (6);
    area.removeFromBottom (kMargin);

    // The scrollbar sits in the right margin, so the columns never move away from their headers.
    viewport.setBounds (area.withWidth (area.getWidth() + viewport.getScrollBarThickness()));

    const int count = (int) rows.size();
    content.setSize (area.getWidth(), count > 0 ? count * (kRowHeight + kRowGap) - kRowGap : 0);

    for (int i = 0; i < count; ++i)
        rows[(size_t) i]->setBounds (0, i * (kRowHeight + kRowGap), area.getWidth(), kRowHeight);
}
