#pragma once

#include <JuceHeader.h>

namespace s77::ui
{

namespace Colours
{
    inline const juce::Colour window     { 0xff070708 };
    inline const juce::Colour panel      { 0xff0c0c0e };
    inline const juce::Colour surface    { 0xff131316 };
    inline const juce::Colour surfaceHi  { 0xff1a1a1e };
    inline const juce::Colour line       { 0xff1e1e22 };
    inline const juce::Colour lineStrong { 0xff2b2b31 };
    inline const juce::Colour text       { 0xffededee };
    inline const juce::Colour textDim    { 0xff8d8d95 };
    inline const juce::Colour textFaint  { 0xff55555d };
    inline const juce::Colour accent     { 0xfff2f2f2 };
    inline const juce::Colour reference  { 0xff6f8fab };
    inline const juce::Colour blob       { 0xff2a2a30 };
    inline const juce::Colour blobEdge   { 0xff4a4a52 };
    inline const juce::Colour live       { 0xff7fbf8e };
}

class Fonts
{
public:
    enum Weight { Regular, Medium, SemiBold, Bold };

    static juce::Font get (float height, Weight w = Regular)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (height));
    }

    static juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr faces[4] = {
            juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,  BinaryData::InterRegular_otfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf,   BinaryData::InterMedium_otfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf,     BinaryData::InterBold_otfSize)
        };
        return faces[(int) w];
    }
};

/** Flat, monochrome look. No glows, no hover states. */
class LookAndFeel77 : public juce::LookAndFeel_V4
{
public:
    LookAndFeel77()
    {
        using namespace Colours;
        setColour (juce::ResizableWindow::backgroundColourId, window);
        setColour (juce::ComboBox::backgroundColourId, surface);
        setColour (juce::ComboBox::outlineColourId, lineStrong);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, surface);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, surfaceHi);
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TextButton::buttonColourId, surface);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, window);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ScrollBar::thumbColourId, lineStrong);
        setColour (juce::TooltipWindow::backgroundColourId, surfaceHi);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, lineStrong);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        return Fonts::typeface (f.isBold() ? Fonts::SemiBold : Fonts::Regular);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace Colours;
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcR = radius - 2.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (lineStrong);
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? accent : textFaint);
        g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float knobR = arcR - 7.0f;
        g.setColour (surface);
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (lineStrong);
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        const juce::Point<float> p1 (centre.x + (knobR * 0.35f) * std::sin (angle), centre.y - (knobR * 0.35f) * std::cos (angle));
        const juce::Point<float> p2 (centre.x + (knobR - 3.0f) * std::sin (angle), centre.y - (knobR - 3.0f) * std::cos (angle));
        g.setColour (accent);
        g.drawLine ({ p1, p2 }, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool, bool isDown) override
    {
        using namespace Colours;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        g.setColour (on ? accent : (isDown ? surfaceHi : surface));
        g.fillRoundedRectangle (r, 4.0f);
        if (! on)
        {
            g.setColour (lineStrong);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return Fonts::get (juce::jmin (11.5f, (float) h * 0.48f), Fonts::SemiBold);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? Colours::window : (b.isEnabled() ? Colours::text : Colours::textFaint));
        g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace Colours;
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (surface);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (lineStrong);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (textDim);
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::get (12.5f, Fonts::Medium); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return Fonts::get (13.0f, Fonts::Regular); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Colours::surface);
        g.setColour (Colours::lineStrong);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool isVertical,
                        int thumbStart, int thumbSize, bool, bool) override
    {
        g.setColour (Colours::panel);
        g.fillRect (x, y, width, height);
        auto thumb = isVertical ? juce::Rectangle<int> (x + 3, thumbStart, width - 6, thumbSize)
                                : juce::Rectangle<int> (thumbStart, y + 3, thumbSize, height - 6);
        g.setColour (Colours::lineStrong);
        g.fillRoundedRectangle (thumb.toFloat(), 2.5f);
    }

    int getDefaultScrollbarWidth() override { return 10; }
};

/** Rotary control with caption and value read-out. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
        : name (caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setVelocityBasedMode (false);
        slider.setMouseDragSensitivity (220);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

        if (auto* p = state.getParameter (paramId))
        {
            param = p;
            slider.onValueChange = [this] { repaint(); };
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (16);
        r.removeFromBottom (16);
        slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (Colours::textDim);
        g.setFont (Fonts::get (10.5f, Fonts::SemiBold));
        g.drawText (name.toUpperCase(), r.removeFromTop (14), juce::Justification::centred);

        g.setColour (Colours::text);
        g.setFont (Fonts::get (11.5f, Fonts::Medium));
        const auto txt = param != nullptr ? param->getCurrentValueAsText() : juce::String (slider.getValue());
        g.drawText (txt, r.removeFromBottom (14), juce::Justification::centred);
    }

private:
    juce::String name;
    juce::Slider slider;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Clickable credit without any hover styling. */
class CreditLink : public juce::Component
{
public:
    CreditLink (juce::String textToShow, juce::URL target) : text (std::move (textToShow)), url (std::move (target))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colours::textDim);
        g.setFont (Fonts::get (11.0f, Fonts::Medium));
        g.drawText (text, getLocalBounds(), juce::Justification::centredRight);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
            url.launchInDefaultBrowser();
    }

private:
    juce::String text;
    juce::URL url;
};

inline juce::String midiToName (float midi, bool withCents = false)
{
    if (midi <= 0.0f) return "--";
    const int n = juce::roundToInt (midi);
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (withCents)
    {
        const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
        s << (cents >= 0 ? " +" : " ") << cents;
    }
    return s;
}

} // namespace s77::ui
