#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/PianoRoll.h"
#include "ui/AutoTunePage.h"

namespace s77::ui
{

/**
    Two-engine switch. Clicking an engine turns it on; clicking the power glyph of the
    running engine turns it off, which hands over to the other engine.
*/
class EngineSwitch : public juce::Component
{
public:
    explicit EngineSwitch (juce::RangedAudioParameter& p)
        : param (p), attachment (p, [this] (float) { repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int current() const { return juce::roundToInt (param.convertFrom0to1 (param.getValue())); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::surface);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::lineStrong);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        static const char* labels[] = { "VOCAL DRAW", "AUTO-TUNE" };
        for (int i = 0; i < 2; ++i)
        {
            auto seg = segment (i).toFloat().reduced (3.0f);
            const bool on = current() == i;
            if (on)
            {
                g.setColour (Colours::accent);
                g.fillRoundedRectangle (seg, 4.0f);
            }

            auto power = powerArea (i).toFloat();
            drawPower (g, power, on ? Colours::window : Colours::textFaint);

            g.setColour (on ? Colours::window : Colours::textDim);
            g.setFont (Fonts::get (11.5f, Fonts::Bold));
            g.drawText (labels[i], seg.withTrimmedLeft (power.getRight() - seg.getX() + 4.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        const int clicked = e.x < getWidth() / 2 ? 0 : 1;
        int next = clicked;
        if (clicked == current() && powerArea (clicked).expanded (6).contains (e.getPosition()))
            next = 1 - clicked;   // power off -> the other engine takes over
        attachment.setValueAsCompleteGesture ((float) next);
    }

private:
    juce::Rectangle<int> segment (int i) const
    {
        auto r = getLocalBounds();
        return i == 0 ? r.removeFromLeft (getWidth() / 2) : r.withTrimmedLeft (getWidth() / 2);
    }

    juce::Rectangle<int> powerArea (int i) const
    {
        auto s = segment (i).reduced (3);
        return s.removeFromLeft (s.getHeight()).reduced (7).translated (4, 0);
    }

    static void drawPower (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        auto b = r.withSizeKeepingCentre (s, s);
        juce::Path p;
        p.addCentredArc (b.getCentreX(), b.getCentreY(), s * 0.45f, s * 0.45f, 0.0f,
                         juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
        p.startNewSubPath (b.getCentreX(), b.getY() - 0.5f);
        p.lineTo (b.getCentreX(), b.getCentreY());
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
};

/** Vocal Draw page: tools, piano roll, controls. */
class VocalDrawPage : public juce::Component
{
public:
    explicit VocalDrawPage (SeventySevenProcessor& p);

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void selectTool (PianoRoll::Tool t);
    void styleButton (juce::TextButton& b, const juce::String& tip);

    SeventySevenProcessor& proc;
    PianoRoll roll;

    juce::TextButton selectBtn { "Select" }, pencilBtn { "Pencil" }, lineBtn { "Line" }, eraseBtn { "Erase" };
    juce::TextButton snapBtn { "Snap Notes" }, refBtn { "From Reference" }, undoBtn { "Undo" }, clearBtn { "Clear" }, fitBtn { "Fit" };
    juce::TextButton autoDrawBtn { "Auto Draw" }, refSnapBtn { "Snap Ref" }, followBtn { "Follow" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoDrawAttach, refSnapAttach;

    Knob correction, transition, variation, mix, output;
    juce::Rectangle<int> legendArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalDrawPage)
};

} // namespace s77::ui

//==============================================================================
class SeventySevenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeventySevenEditor (SeventySevenProcessor&);
    ~SeventySevenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updatePage();

    SeventySevenProcessor& proc;
    s77::ui::LookAndFeel77 lnf;

    juce::Image logo;
    s77::ui::EngineSwitch engineSwitch;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttach, scaleAttach;

    s77::ui::VocalDrawPage drawPage;
    s77::ui::AutoTunePage tunePage;
    s77::ui::CreditLink credit;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> headerArea, footerArea, statusArea, logoArea;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeventySevenEditor)
};
