#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

namespace s77::ui
{

/**
    Newtone-style pitch editor.

    Grey blobs   : notes detected on the vocal (input)
    Blue line    : reference vocal from the side-chain
    White line   : correction target (drawn, snapped or auto-drawn)
*/
class PianoRoll : public juce::Component
{
public:
    enum class Tool { Select, Pencil, Line, Erase };

    explicit PianoRoll (SeventySevenProcessor& p);

    void setTool (Tool t) { tool = t; updateCursor(); repaint(); }
    Tool getTool() const  { return tool; }
    void setFollowPlayhead (bool f) { follow = f; }

    // whole-take operations
    void snapAllNotes();
    void drawFromReference();
    void clearTarget();
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void zoomToFit();

    void refresh();   // called from the editor timer

    std::function<void()> onEdit;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    static constexpr int keysWidth = 54;
    static constexpr int rulerHeight = 22;
    static constexpr float minMidi = 24.0f, maxMidi = 96.0f;

    juce::Rectangle<int> gridArea() const;
    float xForTime (double sec) const;
    double timeForX (float x) const;
    float yForMidi (float midi) const;
    float midiForY (float y) const;
    int frameForX (float x) const;
    float xForFrame (int f) const;

    void drawGrid (juce::Graphics&, juce::Rectangle<int> area, uint32_t mask);
    void drawKeys (juce::Graphics&, juce::Rectangle<int> area, uint32_t mask);
    void drawRuler (juce::Graphics&, juce::Rectangle<int> area);
    void drawCurves (juce::Graphics&, juce::Rectangle<int> area);

    void pushUndo();
    void writeSegment (const s77::PitchTimeline::Segment& seg, float offsetSemis, bool snapCentre);
    void writeLine (int f0, float m0, int f1, float m1);
    void eraseRange (int f0, int f1);
    void updateCursor();
    void clampView();
    float variation() const;

    SeventySevenProcessor& proc;
    s77::PitchTimeline& tl;

    Tool tool = Tool::Select;
    bool follow = true;

    double viewStart = 0.0;     // seconds at the left edge of the grid
    double pxPerSec = 140.0;
    float topMidi = 76.0f;      // note at the top edge of the grid
    float rowHeight = 14.0f;    // pixels per semitone

    // interaction state
    enum class Drag { None, Pan, Note, Pencil, Line, Erase } drag = Drag::None;
    s77::PitchTimeline::Segment dragSeg;
    float dragBaseOffset = 0.0f;
    float lastAppliedOffset = 0.0f;
    juce::Point<float> dragStart, lastPos;
    double panStartTime = 0.0;
    float panStartMidi = 0.0f;
    bool hasSelection = false;
    s77::PitchTimeline::Segment selected;

    std::vector<std::vector<float>> undoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};

} // namespace s77::ui
