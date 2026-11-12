#include "PluginEditor.h"

namespace s77::ui
{

//==============================================================================
VocalDrawPage::VocalDrawPage (SeventySevenProcessor& p)
    : proc (p), roll (p),
      correction (p.apvts, ParamIDs::drawAmount, "Correction"),
      transition (p.apvts, ParamIDs::drawSmooth, "Transition"),
      variation (p.apvts, ParamIDs::variation, "Variation"),
      mix (p.apvts, ParamIDs::mix, "Mix"),
      output (p.apvts, ParamIDs::output, "Output")
{
    setOpaque (true);
    addAndMakeVisible (roll);

    styleButton (selectBtn, "Select / move notes (1). Drag a note up or down, Alt for fine cents, double-click to snap.");
    styleButton (pencilBtn, "Draw the pitch curve freehand (2). Hold Shift to lock to semitones.");
    styleButton (lineBtn,   "Draw a straight pitch line (3). Hold Shift to lock to semitones.");
    styleButton (eraseBtn,  "Erase corrections (4).");
    for (auto* b : { &selectBtn, &pencilBtn, &lineBtn, &eraseBtn })
    {
        b->setRadioGroupId (7701);
        b->setClickingTogglesState (true);
    }
    selectBtn.setToggleState (true, juce::dontSendNotification);
    selectBtn.onClick = [this] { selectTool (PianoRoll::Tool::Select); };
    pencilBtn.onClick = [this] { selectTool (PianoRoll::Tool::Pencil); };
    lineBtn.onClick   = [this] { selectTool (PianoRoll::Tool::Line); };
    eraseBtn.onClick  = [this] { selectTool (PianoRoll::Tool::Erase); };

    styleButton (snapBtn,  "Snap every detected note to the selected key and scale.");
    styleButton (refBtn,   "Draw the whole take from the side-chained reference vocal.");
    styleButton (undoBtn,  "Undo the last edit (Ctrl+Z).");
    styleButton (clearBtn, "Remove all corrections.");
    styleButton (fitBtn,   "Zoom to fit the captured vocal.");
    snapBtn.onClick  = [this] { roll.snapAllNotes(); };
    refBtn.onClick   = [this] { roll.drawFromReference(); };
    undoBtn.onClick  = [this] { roll.undo(); };
    clearBtn.onClick = [this] { roll.clearTarget(); };
    fitBtn.onClick   = [this] { roll.zoomToFit(); };

    styleButton (autoDrawBtn, "Follow the side-chained reference vocal and draw the correction automatically while playing.");
    styleButton (refSnapBtn,  "Snap the reference melody to the selected key and scale.");
    styleButton (followBtn,   "Scroll the view with the playhead.");
    for (auto* b : { &autoDrawBtn, &refSnapBtn, &followBtn })
        b->setClickingTogglesState (true);
    autoDrawAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::autoDraw, autoDrawBtn);
    refSnapAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::refSnap, refSnapBtn);
    followBtn.setToggleState (true, juce::dontSendNotification);
    followBtn.onClick = [this] { roll.setFollowPlayhead (followBtn.getToggleState()); };

    roll.onEdit = [this]
    {
        const auto t = roll.getTool();
        selectBtn.setToggleState (t == PianoRoll::Tool::Select, juce::dontSendNotification);
        pencilBtn.setToggleState (t == PianoRoll::Tool::Pencil, juce::dontSendNotification);
        lineBtn.setToggleState (t == PianoRoll::Tool::Line, juce::dontSendNotification);
        eraseBtn.setToggleState (t == PianoRoll::Tool::Erase, juce::dontSendNotification);
        undoBtn.setEnabled (roll.canUndo());
    };
    undoBtn.setEnabled (false);

    for (auto* k : { &correction, &transition, &variation, &mix, &output })
        addAndMakeVisible (*k);
}

void VocalDrawPage::styleButton (juce::TextButton& b, const juce::String& tip)
{
    b.setTooltip (tip);
    b.setWantsKeyboardFocus (false);
    addAndMakeVisible (b);
}

void VocalDrawPage::selectTool (PianoRoll::Tool t)
{
    roll.setTool (t);
}

void VocalDrawPage::refresh()
{
    roll.refresh();
    undoBtn.setEnabled (roll.canUndo());
}

void VocalDrawPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);

    auto bar = r.removeFromTop (30);
    auto place = [&bar] (juce::Button& b, int w) { b.setBounds (bar.removeFromLeft (w)); bar.removeFromLeft (4); };
    place (selectBtn, 72); place (pencilBtn, 72); place (lineBtn, 60); place (eraseBtn, 66);
    bar.removeFromLeft (14);
    place (snapBtn, 102); place (refBtn, 124); place (undoBtn, 62); place (clearBtn, 62); place (fitBtn, 48);

    auto right = bar;
    followBtn.setBounds (right.removeFromRight (70));
    right.removeFromRight (4);
    refSnapBtn.setBounds (right.removeFromRight (84));
    right.removeFromRight (4);
    autoDrawBtn.setBounds (right.removeFromRight (92));

    r.removeFromTop (10);
    auto bottom = r.removeFromBottom (104);
    r.removeFromBottom (12);
    roll.setBounds (r);

    auto knobs = bottom.removeFromLeft (5 * 92);
    for (auto* k : { &correction, &transition, &variation, &mix, &output })
        k->setBounds (knobs.removeFromLeft (92).reduced (4, 0));
    legendArea = bottom.withTrimmedLeft (24);
}

void VocalDrawPage::paint (juce::Graphics& g)
{
    g.fillAll (Colours::window);

    auto r = legendArea.withSizeKeepingCentre (legendArea.getWidth(), 76);
    auto item = [&] (juce::Colour c, float thickness, const juce::String& name, const juce::String& desc)
    {
        auto line = r.removeFromTop (24);
        auto swatch = line.removeFromLeft (26).toFloat();
        g.setColour (c);
        g.fillRoundedRectangle (swatch.withSizeKeepingCentre (18.0f, thickness), thickness * 0.5f);
        g.setColour (Colours::text);
        g.setFont (Fonts::get (11.5f, Fonts::SemiBold));
        g.drawText (name, line.removeFromLeft (92), juce::Justification::centredLeft);
        g.setColour (Colours::textFaint);
        g.setFont (Fonts::get (11.5f));
        g.drawText (desc, line, juce::Justification::centredLeft);
    };

    const bool sc = proc.telemetry.sidechainSignal.load();
    item (Colours::blobEdge, 8.0f, "Your vocal", "Notes detected on this track");
    item (Colours::reference, 2.0f, "Reference", sc ? "Side-chain vocal - receiving" : "Side-chain vocal - not connected");
    item (Colours::accent, 2.0f, "Correction", "Pitch the vocal is tuned to");
}

} // namespace s77::ui

//==============================================================================
SeventySevenEditor::SeventySevenEditor (SeventySevenProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      engineSwitch (*p.apvts.getParameter (ParamIDs::mode)),
      drawPage (p), tunePage (p),
      credit ("By WajFay", juce::URL (SeventySevenProcessor::instagramUrl))
{
    setLookAndFeel (&lnf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_mark_png, BinaryData::logo_mark_pngSize);

    addAndMakeVisible (engineSwitch);

    keyBox.addItemList (p.apvts.getParameter (ParamIDs::key)->getAllValueStrings(), 1);
    scaleBox.addItemList (p.apvts.getParameter (ParamIDs::scale)->getAllValueStrings(), 1);
    keyBox.setTooltip ("Key");
    scaleBox.setTooltip ("Scale");
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::key, keyBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::scale, scaleBox);

    addChildComponent (drawPage);
    addChildComponent (tunePage);
    addAndMakeVisible (credit);

    setResizable (true, true);
    setResizeLimits (1000, 640, 2000, 1300);
    setSize (1140, 740);

    updatePage();
    startTimerHz (30);
}

SeventySevenEditor::~SeventySevenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SeventySevenEditor::updatePage()
{
    const int mode = engineSwitch.current();
    if (mode == lastMode) return;
    lastMode = mode;
    drawPage.setVisible (mode == (int) EngineMode::VocalDraw);
    tunePage.setVisible (mode == (int) EngineMode::AutoTune);
}

void SeventySevenEditor::timerCallback()
{
    updatePage();
    if (drawPage.isVisible()) drawPage.refresh();
    if (tunePage.isVisible()) tunePage.refresh();
    repaint (statusArea);
    repaint (footerArea);
}

void SeventySevenEditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (64);
    footerArea = r.removeFromBottom (26);
    drawPage.setBounds (r);
    tunePage.setBounds (r);

    auto h = headerArea.reduced (18, 14);
    logoArea = h.removeFromLeft (220);

    auto right = h.removeFromRight (420);
    scaleBox.setBounds (right.removeFromRight (150).withSizeKeepingCentre (150, 30));
    right.removeFromRight (8);
    keyBox.setBounds (right.removeFromRight (70).withSizeKeepingCentre (70, 30));
    right.removeFromRight (16);
    statusArea = right;

    engineSwitch.setBounds (h.withSizeKeepingCentre (juce::jmin (300, h.getWidth()), 36));

    credit.setBounds (footerArea.reduced (18, 0).removeFromRight (140));
}

void SeventySevenEditor::paint (juce::Graphics& g)
{
    namespace Colours = s77::ui::Colours;
    using s77::ui::Fonts;
    g.fillAll (Colours::window);

    // header
    g.setColour (Colours::panel);
    g.fillRect (headerArea);
    g.setColour (Colours::line);
    g.fillRect (headerArea.getX(), headerArea.getBottom() - 1, headerArea.getWidth(), 1);

    auto l = logoArea;
    if (logo.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (logo, l.removeFromLeft (40).toFloat(), juce::RectanglePlacement::centred);
    }
    l.removeFromLeft (12);
    g.setColour (Colours::text);
    g.setFont (Fonts::get (20.0f, Fonts::Bold));
    g.drawText ("77.MP3", l.removeFromTop (l.getHeight() / 2 + 3), juce::Justification::bottomLeft);
    g.setColour (Colours::textFaint);
    g.setFont (Fonts::get (10.0f, Fonts::SemiBold));
    g.drawText ("VOCAL ENGINE", l, juce::Justification::topLeft);

    // side-chain status
    {
        auto s = statusArea;
        const bool connected = proc.telemetry.sidechainConnected.load();
        const bool signal = proc.telemetry.sidechainSignal.load();
        auto top = s.removeFromTop (s.getHeight() / 2);
        g.setColour (Colours::textFaint);
        g.setFont (Fonts::get (10.0f, Fonts::SemiBold));
        g.drawText ("SIDE-CHAIN", top, juce::Justification::bottomRight);

        const juce::String state = signal ? "RECEIVING" : connected ? "NO SIGNAL" : "NOT ROUTED";
        g.setFont (Fonts::get (11.5f, Fonts::SemiBold));
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), state);
        g.setColour (signal ? Colours::text : Colours::textDim);
        g.drawText (state, s, juce::Justification::topRight);
        g.setColour (signal ? Colours::live : Colours::lineStrong);
        g.fillEllipse ((float) (s.getRight() - tw - 12), (float) s.getY() + 4.0f, 6.0f, 6.0f);
    }

    // footer
    g.setColour (Colours::panel);
    g.fillRect (footerArea);
    g.setColour (Colours::line);
    g.fillRect (footerArea.getX(), footerArea.getY(), footerArea.getWidth(), 1);

    const double sr = proc.telemetry.sampleRate.load();
    const double latencyMs = proc.telemetry.latencySamples.load() * 1000.0 / juce::jmax (1.0, sr);
    g.setColour (Colours::textFaint);
    g.setFont (Fonts::get (10.5f, Fonts::Medium));
    g.drawText ("77.MP3  v" JucePlugin_VersionString "   |   " + juce::String (sr / 1000.0, 1) + " kHz   |   Latency " + juce::String (latencyMs, 1) + " ms (auto-compensated)",
                footerArea.reduced (18, 0), juce::Justification::centredLeft);
}
