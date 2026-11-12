#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

namespace s77::ui
{

/** Real-time pitch correction view: detected note, cents meter, pitch history, scale keys. */
class AutoTunePage : public juce::Component
{
public:
    explicit AutoTunePage (SeventySevenProcessor& p)
        : proc (p),
          retune (p.apvts, ParamIDs::retune, "Retune Speed"),
          humanize (p.apvts, ParamIDs::humanize, "Humanize"),
          mix (p.apvts, ParamIDs::mix, "Mix"),
          output (p.apvts, ParamIDs::output, "Output")
    {
        for (auto* k : { &retune, &humanize, &mix, &output })
            addAndMakeVisible (*k);
        setOpaque (true);
    }

    void refresh() { repaint (infoArea.getUnion (graphArea).getUnion (keysArea)); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);

        auto bottom = r.removeFromBottom (112);
        auto knobs = bottom.removeFromRight (4 * 96);
        for (auto* k : { &retune, &humanize, &mix, &output })
            k->setBounds (knobs.removeFromLeft (96).reduced (4, 0));
        bottom.removeFromRight (16);
        keysArea = bottom.withSizeKeepingCentre (bottom.getWidth(), 64).withY (bottom.getY() + 30);
        keysCaption = keysArea.withHeight (16).translated (0, -22);

        r.removeFromBottom (14);
        infoArea = r.removeFromLeft (260);
        r.removeFromLeft (14);
        graphArea = r;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::window);

        const float inMidi = proc.telemetry.inputMidi.load();
        const float outMidi = proc.telemetry.outputMidi.load();
        const float shift = proc.telemetry.shiftSemis.load();
        const uint32_t mask = proc.currentScaleMask();

        paintInfo (g, inMidi, outMidi, shift);
        paintGraph (g, mask);
        paintKeys (g, mask, inMidi > 0.0f ? s77::nearestAllowed (inMidi, mask) : -1);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! keysArea.contains (e.getPosition()))
            return;

        const int pc = juce::jlimit (0, 11, (e.x - keysArea.getX()) * 12 / juce::jmax (1, keysArea.getWidth()));
        uint32_t mask = proc.currentScaleMask() ^ (1u << pc);
        if ((mask & 0xFFFu) == 0)
            return; // keep at least one note

        proc.setCustomMask (mask);
        if (auto* p = proc.apvts.getParameter (ParamIDs::scale))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) s77::ScaleType::Custom));
        repaint();
    }

private:
    void card (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

    void paintInfo (juce::Graphics& g, float inMidi, float outMidi, float shift)
    {
        card (g, infoArea);
        auto r = infoArea.reduced (18, 16);

        g.setColour (Colours::textDim);
        g.setFont (Fonts::get (10.5f, Fonts::SemiBold));
        g.drawText ("DETECTED", r.removeFromTop (14), juce::Justification::centredLeft);

        g.setColour (inMidi > 0.0f ? Colours::text : Colours::textFaint);
        g.setFont (Fonts::get (54.0f, Fonts::Bold));
        g.drawText (midiToName (inMidi), r.removeFromTop (64), juce::Justification::centredLeft);

        // cents meter
        auto meter = r.removeFromTop (34);
        auto bar = meter.removeFromTop (8).toFloat();
        g.setColour (Colours::surface);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (Colours::lineStrong);
        g.fillRect (juce::Rectangle<float> (bar.getCentreX() - 0.5f, bar.getY() - 3.0f, 1.0f, bar.getHeight() + 6.0f));

        int cents = 0;
        if (inMidi > 0.0f)
        {
            cents = juce::roundToInt ((inMidi - std::round (inMidi)) * 100.0f);
            const float x = bar.getCentreX() + bar.getWidth() * 0.5f * (float) cents / 50.0f;
            g.setColour (Colours::accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (x - 2.0f, bar.getY() - 2.0f, 4.0f, bar.getHeight() + 4.0f), 2.0f);
        }
        g.setColour (Colours::textFaint);
        g.setFont (Fonts::get (10.0f, Fonts::Medium));
        auto labels = meter.removeFromTop (16);
        g.drawText ("-50", labels, juce::Justification::centredLeft);
        g.drawText (inMidi > 0.0f ? (cents >= 0 ? "+" : "") + juce::String (cents) + " ct" : "--", labels, juce::Justification::centred);
        g.drawText ("+50", labels, juce::Justification::centredRight);

        r.removeFromTop (14);
        auto row = [&] (const juce::String& label, const juce::String& value)
        {
            auto line = r.removeFromTop (26);
            g.setColour (Colours::textDim);
            g.setFont (Fonts::get (11.0f, Fonts::SemiBold));
            g.drawText (label, line, juce::Justification::centredLeft);
            g.setColour (Colours::text);
            g.setFont (Fonts::get (13.0f, Fonts::Medium));
            g.drawText (value, line, juce::Justification::centredRight);
            g.setColour (Colours::line);
            g.fillRect (line.removeFromBottom (1));
        };

        row ("OUTPUT", midiToName (outMidi, true));
        row ("CORRECTION", inMidi > 0.0f ? (shift >= 0 ? "+" : "") + juce::String (shift * 100.0f, 0) + " ct" : "--");
        const auto key = juce::String (s77::keyNames()[juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::key)->load())]);
        const auto scale = juce::String (s77::scaleNames()[juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::scale)->load())]);
        row ("SCALE", scale == "Custom" || scale == "Chromatic" ? scale : key + " " + scale);
        row ("FREQUENCY", inMidi > 0.0f ? juce::String (440.0f * std::pow (2.0f, (inMidi - 69.0f) / 12.0f), 1) + " Hz" : "--");

        // tuned-to block, anchored to the bottom of the card
        auto bottom = infoArea.reduced (18, 16).removeFromBottom (86);
        if (bottom.getY() > r.getY())
        {
            g.setColour (Colours::line);
            g.fillRect (bottom.removeFromTop (1));
            bottom.removeFromTop (10);
            g.setColour (Colours::textDim);
            g.setFont (Fonts::get (10.5f, Fonts::SemiBold));
            g.drawText ("TUNED TO", bottom.removeFromTop (14), juce::Justification::centredLeft);
            g.setColour (outMidi > 0.0f ? Colours::accent : Colours::textFaint);
            g.setFont (Fonts::get (40.0f, Fonts::Bold));
            g.drawText (outMidi > 0.0f ? midiToName (std::round (outMidi)) : "--", bottom, juce::Justification::centredLeft);
        }
    }

    void paintGraph (juce::Graphics& g, uint32_t mask)
    {
        card (g, graphArea);
        auto area = graphArea.reduced (1);

        constexpr int N = SeventySevenProcessor::Telemetry::graphSize;
        const int w = proc.telemetry.graphWrite.load (std::memory_order_acquire);
        std::array<float, N> in {}, out {};
        float lo = 1000.0f, hi = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const int idx = (w + i) % N;
            in[(size_t) i] = proc.telemetry.graphIn[idx].load (std::memory_order_relaxed);
            out[(size_t) i] = proc.telemetry.graphOut[idx].load (std::memory_order_relaxed);
            if (in[(size_t) i] > 0.0f) { lo = juce::jmin (lo, in[(size_t) i], out[(size_t) i]); hi = juce::jmax (hi, in[(size_t) i], out[(size_t) i]); }
        }

        float centre = hi > 0.0f ? (lo + hi) * 0.5f : 60.0f;
        const float span = hi > 0.0f ? juce::jmax (14.0f, hi - lo + 4.0f) : 14.0f;
        smoothCentre += (centre - smoothCentre) * 0.15f;
        smoothSpan += (span - smoothSpan) * 0.15f;

        const float top = smoothCentre + smoothSpan * 0.5f;
        const float rowH = (float) area.getHeight() / smoothSpan;
        auto yFor = [&] (float m) { return (float) area.getY() + (top - m) * rowH; };

        g.saveState();
        g.reduceClipRegion (area);

        for (int n = (int) std::floor (top - smoothSpan) - 1; n <= (int) std::ceil (top) + 1; ++n)
        {
            const float y = yFor ((float) n);
            const bool inScale = (mask & (1u << s77::pitchClass (n))) != 0;
            g.setColour (inScale ? Colours::lineStrong : Colours::line);
            g.fillRect (juce::Rectangle<float> ((float) area.getX() + 40.0f, std::round (y), (float) area.getWidth() - 40.0f, 1.0f));
            if (inScale && rowH >= 9.0f)
            {
                g.setColour (Colours::textFaint);
                g.setFont (Fonts::get (10.0f, Fonts::SemiBold));
                g.drawText (midiToName ((float) n), juce::Rectangle<float> ((float) area.getX() + 8.0f, y - 7.0f, 30.0f, 14.0f).toNearestInt(),
                            juce::Justification::centredLeft);
            }
        }

        auto curve = [&] (const std::array<float, N>& data)
        {
            juce::Path p;
            bool open = false;
            const float x0 = (float) area.getX() + 40.0f;
            const float dx = ((float) area.getWidth() - 48.0f) / (float) (N - 1);
            for (int i = 0; i < N; ++i)
            {
                const float v = data[(size_t) i];
                if (v <= 0.0f) { open = false; continue; }
                const juce::Point<float> pt (x0 + dx * (float) i, yFor (v));
                if (! open) { p.startNewSubPath (pt); open = true; } else p.lineTo (pt);
            }
            return p;
        };

        g.setColour (Colours::textFaint);
        g.strokePath (curve (in), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Colours::accent);
        g.strokePath (curve (out), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // legend
        auto legend = juce::Rectangle<int> (area.getRight() - 190, area.getY() + 10, 180, 14);
        g.setFont (Fonts::get (10.5f, Fonts::SemiBold));
        g.setColour (Colours::textFaint);
        g.fillRect (legend.getX(), legend.getCentreY(), 14, 2);
        g.setColour (Colours::textDim);
        g.drawText ("INPUT", legend.withTrimmedLeft (20).withWidth (60), juce::Justification::centredLeft);
        g.setColour (Colours::accent);
        g.fillRect (legend.getX() + 84, legend.getCentreY(), 14, 2);
        g.setColour (Colours::textDim);
        g.drawText ("CORRECTED", legend.withTrimmedLeft (104), juce::Justification::centredLeft);

        g.restoreState();
    }

    void paintKeys (juce::Graphics& g, uint32_t mask, int activeNote)
    {
        g.setColour (Colours::textDim);
        g.setFont (Fonts::get (10.5f, Fonts::SemiBold));
        g.drawText ("SCALE NOTES  -  CLICK TO TOGGLE", keysCaption, juce::Justification::centredLeft);

        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float cw = (float) keysArea.getWidth() / 12.0f;
        for (int i = 0; i < 12; ++i)
        {
            auto cell = juce::Rectangle<float> ((float) keysArea.getX() + cw * (float) i, (float) keysArea.getY(), cw, (float) keysArea.getHeight()).reduced (2.0f, 0.0f);
            const bool on = (mask & (1u << i)) != 0;
            const bool black = i == 1 || i == 3 || i == 6 || i == 8 || i == 10;
            const bool active = activeNote >= 0 && s77::pitchClass (activeNote) == i;

            g.setColour (on ? (black ? juce::Colour (0xffbdbdc2) : Colours::accent) : Colours::surface);
            g.fillRoundedRectangle (cell, 4.0f);
            if (! on)
            {
                g.setColour (Colours::lineStrong);
                g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);
            }
            if (active)
            {
                g.setColour (on ? Colours::window : Colours::text);
                g.fillRoundedRectangle (cell.withHeight (3.0f).withY (cell.getBottom() - 8.0f).reduced (cell.getWidth() * 0.3f, 0.0f), 1.5f);
            }

            g.setColour (on ? Colours::window : Colours::textFaint);
            g.setFont (Fonts::get (12.0f, Fonts::SemiBold));
            g.drawText (names[i], cell.withTrimmedBottom (10.0f).toNearestInt(), juce::Justification::centred);
        }
    }

    SeventySevenProcessor& proc;
    Knob retune, humanize, mix, output;
    juce::Rectangle<int> infoArea, graphArea, keysArea, keysCaption;
    float smoothCentre = 60.0f, smoothSpan = 14.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoTunePage)
};

} // namespace s77::ui
