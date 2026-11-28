#include "AboutPanel.h"

#include "Brand.h"

using namespace brand;

namespace
{
    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    // Top to bottom, all centred: emblem, word mark, version; what the app does and
    // its earlier names; a short hairline; the licence and the project's address.
    constexpr int kEmblemY = 24, kEmblemSize = 150;
    constexpr int kWordMarkY = 180, kWordMarkH = 36;
    constexpr float kWordMarkHeight = 28.0f, kWordMarkTracking = 0.03f;
    constexpr int kVersionY = 216, kVersionH = 18;
    constexpr int kTaglineY = 248, kLegacyY = 266, kLineH = 18;
    constexpr int kRuleY = 296, kRuleW = 120;
    constexpr int kLicenseY = 308, kLicenseH = 16;
    constexpr int kLinkY = 324, kLinkH = 20, kLinkPadding = 12;

    constexpr float kLinkTextHeight = 12.5f;

    const char* const kProjectUrl = "https://github.com/XLeathnex/VioLogic";
    const char* const kProjectText = "github.com/XLeathnex/VioLogic";

    // The window that is open, if any; there is only ever one.
    juce::Component::SafePointer<juce::DialogWindow> openWindow;

    /** A line across the whole panel, for centred text. */
    juce::Rectangle<int> row (const juce::Component& panel, int y, int height)
    {
        return { 0, y, panel.getWidth(), height };
    }
}

//==============================================================================
AboutPanel::Link::Link()
    : juce::HyperlinkButton (kProjectText, juce::URL (kProjectUrl))
{
    setWantsKeyboardFocus (false);
    setTooltip (U ("\xc3\x96" "ffnet die Projektseite im Browser: alle Versionen mit ihren \xc3\x84nderungen, "
                   "die Anleitung und der Quelltext."));
}

int AboutPanel::Link::textWidth() const
{
    return juce::GlyphArrangement::getStringWidthInt (brand::font (kLinkTextHeight), getButtonText());
}

void AboutPanel::Link::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    // The accent colour, a touch lighter while the mouse is over it; underlined then, as a link is.
    const auto ink = down ? colour::crimson : (highlighted ? colour::crimson.brighter (0.35f) : colour::crimson.brighter (0.15f));
    const auto r = getLocalBounds();
    const auto f = brand::font (kLinkTextHeight);

    g.setColour (ink);
    g.setFont (f);
    g.drawText (getButtonText(), r, juce::Justification::centred, false);

    if (highlighted || down)
    {
        // Just under the baseline of the centred line, whatever the typeface's proportions.
        const float baseline = ((float) r.getHeight() - f.getAscent() - f.getDescent()) * 0.5f + f.getAscent();
        const auto w = (float) textWidth();
        g.fillRect (juce::Rectangle<float> (((float) r.getWidth() - w) * 0.5f, (float) juce::roundToInt (baseline) + 1.0f, w, 1.0f));
    }
}

//==============================================================================
AboutPanel::AboutPanel (const juce::String& version)
    : versionText (version)
{
    setOpaque (true);
    addAndMakeVisible (link);
    setSize (kWidth, kHeight);
}

AboutPanel::~AboutPanel()
{
    if (auto* window = watchedWindow.getComponent())
        window->removeComponentListener (this);
}

void AboutPanel::show (const juce::String& version, juce::Component* centreAround)
{
    if (auto* existing = openWindow.getComponent(); existing != nullptr && existing->isVisible())
    {
        existing->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions launch;
    launch.content.setOwned (new AboutPanel (version));
    launch.dialogTitle = U ("\xc3\x9c" "ber VioLogic");
    launch.dialogBackgroundColour = colour::window;
    launch.componentToCentreAround = centreAround;
    launch.escapeKeyTriggersCloseButton = true;
    launch.useNativeTitleBar = true;
    launch.resizable = false;

    // Not modal, like the About window of any Mac app. The window only hides itself
    // when it is closed; the panel inside then deletes it (componentVisibilityChanged).
    auto* window = launch.create();
    openWindow = window;
    window->setVisible (true);
    window->toFront (true);

    // Over a window that is open modally (the settings, say) it has to be modal too, or
    // it could not be used until that one is closed. Hiding it ends its modal state.
    if (juce::Component::getNumCurrentlyModalComponents() > 0)
        window->enterModalState (true);
}

void AboutPanel::close()
{
    if (auto* window = openWindow.getComponent())
        delete window;
}

void AboutPanel::parentHierarchyChanged()
{
    // Watch the window this panel is in, to notice when it is closed.
    auto* top = getTopLevelComponent();
    if (top == this)
        top = nullptr;

    if (top == watchedWindow.getComponent())
        return;

    if (auto* old = watchedWindow.getComponent())
        old->removeComponentListener (this);

    watchedWindow = top;
    if (top != nullptr)
        top->addComponentListener (this);
}

void AboutPanel::componentVisibilityChanged (juce::Component& window)
{
    if (window.isVisible())
        return;

    // Closed (close button or Escape hide it): delete it from a fresh message, not
    // inside its own call. If it was shown again meanwhile, it stays.
    juce::Component::SafePointer<juce::Component> closed (&window);
    juce::MessageManager::callAsync ([closed]
    {
        if (closed != nullptr && ! closed->isVisible())
            delete closed.getComponent();
    });
}

//==============================================================================
void AboutPanel::resized()
{
    const int w = link.textWidth() + kLinkPadding;
    link.setBounds ((getWidth() - w) / 2, kLinkY, w, kLinkH);
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (colour::window);

    brand::drawEmblem (g, juce::Rectangle<int> ((getWidth() - kEmblemSize) / 2, kEmblemY, kEmblemSize, kEmblemSize).toFloat());

    // The word mark: "Vio" in crimson, "Logic" in silver, as the logo writes it.
    {
        const auto wordFont = brand::font (kWordMarkHeight, true).withExtraKerningFactor (kWordMarkTracking);
        juce::AttributedString s;
        s.setJustification (juce::Justification::centred);
        s.append ("Vio", wordFont, colour::crimson);
        s.append ("Logic", wordFont, colour::silver);

        juce::TextLayout text;
        text.createLayout (s, (float) getWidth());
        text.draw (g, row (*this, kWordMarkY, kWordMarkH).toFloat());
    }

    if (versionText.isNotEmpty())
    {
        g.setFont (brand::font (12.5f));
        g.setColour (colour::silver);
        g.drawText ("Version " + versionText, row (*this, kVersionY, kVersionH), juce::Justification::centred, false);
    }

    g.setFont (brand::font (13.0f));
    g.setColour (colour::text);
    g.drawText (U ("Spielt virtuelle Instrumente mit der Tonh\xc3\xb6he deiner Geige."),
                row (*this, kTaglineY, kLineH), juce::Justification::centred, false);

    g.setFont (brand::font (12.5f));
    g.setColour (colour::secondary);
    g.drawText (U ("Fr\xc3\xbcher Archet und ViolinMIDI."), row (*this, kLegacyY, kLineH), juce::Justification::centred, false);

    g.setColour (colour::hairline);
    g.fillRect ((getWidth() - kRuleW) / 2, kRuleY, kRuleW, 1);

    g.setFont (brand::font (11.5f));
    g.setColour (colour::secondary);
    g.drawText ("Verwendet JUCE. Lizenz: AGPLv3.", row (*this, kLicenseY, kLicenseH), juce::Justification::centred, false);
}

juce::String AboutPanel::getTooltip()
{
    const int y = getMouseXYRelative().y;

    if (y < kVersionY)
        return U ("VioLogic h\xc3\xb6rt \xc3\xbc" "ber das Audio-Interface auf deine Geige und gibt ihre Tonh\xc3\xb6he als MIDI aus. "
                  "MainStage, Logic und andere Programme sehen den Ausgang \xe2\x80\x9eVioLogic\xe2\x80\x9c wie ein angeschlossenes Keyboard.");

    if (y < kVersionY + kVersionH)
        return U ("Die Version, die gerade l\xc3\xa4uft. Eine neuere meldet die App unten im Hauptfenster, "
                  "eine fr\xc3\xbchere l\xc3\xa4sst sich unter \xe2\x80\x9e" "Einstellungen \xe2\x80\xa6\xe2\x80\x9c w\xc3\xa4hlen.");

    if (y >= kTaglineY && y < kLegacyY + kLineH)
        return U ("Bis Version 1.1 hie\xc3\x9f die App ViolinMIDI, von 1.2 bis 1.5 Archet. "
                  "Einstellungen und Presets aus diesen Versionen gelten weiter.");

    if (y >= kLicenseY && y < kLicenseY + kLicenseH)
        return U ("VioLogic ist freie Software unter der GNU AGPLv3 und baut auf dem JUCE-Framework auf. "
                  "Der Quelltext liegt auf der Projektseite.");

    return {};
}
