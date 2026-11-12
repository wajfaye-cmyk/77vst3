#include "PianoRoll.h"

namespace s77::ui
{

using s77::PitchTimeline;

PianoRoll::PianoRoll (SeventySevenProcessor& p) : proc (p), tl (p.timeline)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
    updateCursor();
}

//==============================================================================
juce::Rectangle<int> PianoRoll::gridArea() const
{
    return getLocalBounds().withTrimmedLeft (keysWidth).withTrimmedTop (rulerHeight);
}

float PianoRoll::xForTime (double sec) const   { return (float) gridArea().getX() + (float) ((sec - viewStart) * pxPerSec); }
double PianoRoll::timeForX (float x) const     { return viewStart + (double) (x - (float) gridArea().getX()) / pxPerSec; }
float PianoRoll::yForMidi (float midi) const   { return (float) gridArea().getY() + (topMidi - midi) * rowHeight; }
float PianoRoll::midiForY (float y) const      { return topMidi - (y - (float) gridArea().getY()) / rowHeight; }
int PianoRoll::frameForX (float x) const       { return (int) std::floor (timeForX (x) * PitchTimeline::fps); }
float PianoRoll::xForFrame (int f) const       { return xForTime ((double) f / PitchTimeline::fps); }

float PianoRoll::variation() const
{
    return proc.apvts.getRawParameterValue (ParamIDs::variation)->load() * 0.01f;
}

void PianoRoll::updateCursor()
{
    setMouseCursor (tool == Tool::Select ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
}

void PianoRoll::clampView()
{
    viewStart = juce::jmax (0.0, viewStart);
    pxPerSec = juce::jlimit (8.0, 1600.0, pxPerSec);
    rowHeight = juce::jlimit (6.0f, 40.0f, rowHeight);
    const float visibleRows = (float) gridArea().getHeight() / rowHeight;
    topMidi = juce::jlimit (minMidi + visibleRows, maxMidi + 0.5f, topMidi);
}

void PianoRoll::resized() { clampView(); }

void PianoRoll::refresh()
{
    if (follow && proc.telemetry.playing.load())
    {
        const double ph = proc.telemetry.playheadSeconds.load();
        const double visible = gridArea().getWidth() / pxPerSec;
        if (ph < viewStart || ph > viewStart + visible * 0.85)
            viewStart = juce::jmax (0.0, ph - visible * 0.15);
    }
    repaint();
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);

    const uint32_t mask = proc.currentScaleMask();
    const auto grid = gridArea();

    drawGrid (g, grid, mask);
    drawCurves (g, grid);
    drawKeys (g, { 0, grid.getY(), keysWidth, grid.getHeight() }, mask);
    drawRuler (g, { 0, 0, getWidth(), rulerHeight });

    // playhead
    const double ph = proc.telemetry.playheadSeconds.load();
    const float px = xForTime (ph);
    if (px >= (float) grid.getX() && px <= (float) grid.getRight())
    {
        g.setColour (Colours::accent.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (px - 0.5f, 0.0f, 1.0f, (float) getHeight()));
    }

    if (tl.usedFrames() == 0)
    {
        auto r = grid.withSizeKeepingCentre (520, 60);
        g.setColour (Colours::textDim);
        g.setFont (Fonts::get (14.0f, Fonts::SemiBold));
        g.drawText ("Press play in FL Studio to capture the vocal", r.removeFromTop (24), juce::Justification::centred);
        g.setColour (Colours::textFaint);
        g.setFont (Fonts::get (12.0f));
        g.drawText ("Side-chain a reference vocal into this track to auto draw the melody", r.removeFromTop (22), juce::Justification::centred);
    }

    g.setColour (Colours::line);
    g.drawRect (getLocalBounds(), 1);
}

void PianoRoll::drawGrid (juce::Graphics& g, juce::Rectangle<int> area, uint32_t mask)
{
    g.saveState();
    g.reduceClipRegion (area);

    const int lowNote = (int) std::floor (midiForY ((float) area.getBottom()));
    const int highNote = (int) std::ceil (topMidi);

    for (int n = lowNote; n <= highNote; ++n)
    {
        const float y = yForMidi ((float) n + 0.5f);
        const bool inScale = (mask & (1u << s77::pitchClass (n))) != 0;
        g.setColour (inScale ? juce::Colour (0xff111114) : juce::Colour (0xff0a0a0c));
        g.fillRect (juce::Rectangle<float> ((float) area.getX(), y, (float) area.getWidth(), rowHeight));

        g.setColour (s77::pitchClass (n) == 0 ? Colours::lineStrong : Colours::line);
        g.fillRect (juce::Rectangle<float> ((float) area.getX(), y + rowHeight - 1.0f, (float) area.getWidth(), 1.0f));
    }

    // time grid
    static constexpr double steps[] = { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 };
    double step = 60.0;
    for (double s : steps) if (s * pxPerSec >= 80.0) { step = s; break; }

    const double t0 = std::floor (viewStart / step) * step;
    const double t1 = timeForX ((float) area.getRight());
    for (double t = t0; t <= t1; t += step)
    {
        const float x = xForTime (t);
        g.setColour (Colours::line);
        g.fillRect (juce::Rectangle<float> (std::round (x), (float) area.getY(), 1.0f, (float) area.getHeight()));
    }

    g.restoreState();
}

void PianoRoll::drawKeys (juce::Graphics& g, juce::Rectangle<int> area, uint32_t mask)
{
    g.saveState();
    g.reduceClipRegion (area);
    g.setColour (Colours::surface);
    g.fillRect (area);

    const int lowNote = (int) std::floor (midiForY ((float) area.getBottom()));
    const int highNote = (int) std::ceil (topMidi);

    for (int n = lowNote; n <= highNote; ++n)
    {
        const int pc = s77::pitchClass (n);
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = yForMidi ((float) n + 0.5f);
        auto row = juce::Rectangle<float> ((float) area.getX(), y, (float) area.getWidth(), rowHeight);

        g.setColour (black ? juce::Colour (0xff0b0b0d) : juce::Colour (0xff1b1b1f));
        g.fillRect (row.withWidth (black ? (float) area.getWidth() * 0.62f : (float) area.getWidth()));
        g.setColour (Colours::line);
        g.fillRect (row.withY (row.getBottom() - 1.0f).withHeight (1.0f));

        if ((mask & (1u << pc)) != 0)
        {
            g.setColour (Colours::textDim);
            g.fillRect (juce::Rectangle<float> ((float) area.getRight() - 4.0f, y + 2.0f, 2.0f, rowHeight - 4.0f));
        }

        if (pc == 0 && rowHeight >= 8.0f)
        {
            g.setColour (Colours::textDim);
            g.setFont (Fonts::get (juce::jmin (11.0f, rowHeight - 2.0f), Fonts::SemiBold));
            g.drawText ("C" + juce::String (n / 12 - 1), row.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredRight);
        }
    }

    g.setColour (Colours::lineStrong);
    g.fillRect (area.getRight() - 1, area.getY(), 1, area.getHeight());
    g.restoreState();
}

void PianoRoll::drawRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (Colours::surface);
    g.fillRect (area);
    g.setColour (Colours::lineStrong);
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);

    g.setColour (Colours::textFaint);
    g.setFont (Fonts::get (10.0f, Fonts::SemiBold));
    g.drawText ("TIME", juce::Rectangle<int> (0, 0, keysWidth, area.getHeight()), juce::Justification::centred);

    static constexpr double steps[] = { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 };
    double step = 60.0;
    for (double s : steps) if (s * pxPerSec >= 80.0) { step = s; break; }

    g.saveState();
    g.reduceClipRegion (area.withTrimmedLeft (keysWidth));
    const double t0 = std::floor (viewStart / step) * step;
    const double t1 = timeForX ((float) area.getRight());
    for (double t = t0; t <= t1; t += step)
    {
        const float x = xForTime (t);
        g.setColour (Colours::lineStrong);
        g.fillRect (juce::Rectangle<float> (std::round (x), (float) area.getBottom() - 6.0f, 1.0f, 5.0f));

        const int mins = (int) (t / 60.0);
        const double secs = t - mins * 60.0;
        juce::String label = juce::String (mins) + ":" + (secs < 10.0 ? "0" : "") + juce::String (secs, step < 1.0 ? 2 : 0);
        g.setColour (Colours::textDim);
        g.drawText (label, juce::Rectangle<float> (x + 4.0f, 0.0f, 70.0f, (float) area.getHeight() - 4.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }
    g.restoreState();
}

void PianoRoll::drawCurves (juce::Graphics& g, juce::Rectangle<int> area)
{
    const int used = tl.usedFrames();
    if (used == 0) return;

    g.saveState();
    g.reduceClipRegion (area);

    const int f0 = juce::jmax (0, frameForX ((float) area.getX()) - 1);
    const int f1 = juce::jmin (used, frameForX ((float) area.getRight()) + 2);

    auto buildPath = [&] (auto getter, float maxJump)
    {
        juce::Path path;
        bool open = false;
        float prev = 0.0f;
        for (int f = f0; f < f1; ++f)
        {
            const float v = getter (f);
            if (v <= 0.0f) { open = false; continue; }
            const juce::Point<float> pt (xForFrame (f) + 0.5f * (float) pxPerSec / PitchTimeline::fps, yForMidi (v));
            if (! open || std::abs (v - prev) > maxJump) { path.startNewSubPath (pt); open = true; }
            else path.lineTo (pt);
            prev = v;
        }
        return path;
    };

    // reference vocal
    {
        auto ref = buildPath ([this] (int f) { return tl.ref (f); }, 3.0f);
        g.setColour (Colours::reference.withAlpha (0.85f));
        g.strokePath (ref, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // detected notes (blobs)
    const auto segs = tl.segments (f0 - 300, f1 + 300);
    for (const auto& s : segs)
    {
        if (s.end < f0 || s.start > f1) continue;

        juce::Path p;
        bool open = false;
        for (int f = s.start; f < s.end; ++f)
        {
            const float v = tl.input (f);
            if (v <= 0.0f) continue;
            const juce::Point<float> pt (xForFrame (f) + 0.5f * (float) pxPerSec / PitchTimeline::fps, yForMidi (v));
            if (! open) { p.startNewSubPath (pt); open = true; } else p.lineTo (pt);
        }

        const bool isSel = hasSelection && s.start == selected.start && s.end == selected.end;
        const float thickness = juce::jmax (4.0f, rowHeight * 0.66f);
        g.setColour (isSel ? Colours::blobEdge : Colours::blob);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (isSel ? Colours::text.withAlpha (0.8f) : Colours::textFaint);
        g.strokePath (p, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (rowHeight >= 12.0f && xForFrame (s.end) - xForFrame (s.start) > 34.0f)
        {
            g.setColour (Colours::textDim);
            g.setFont (Fonts::get (10.0f, Fonts::SemiBold));
            g.drawText (midiToName (s.centre), juce::Rectangle<float> (xForFrame (s.start), yForMidi (s.centre) - rowHeight * 1.5f - 2.0f, 60.0f, 12.0f).toNearestInt(),
                        juce::Justification::centredLeft);
        }
    }

    // correction target
    {
        auto target = buildPath ([this] (int f) { return tl.target (f); }, 2.5f);
        g.setColour (Colours::accent);
        g.strokePath (target, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // line tool preview
    if (drag == Drag::Line)
    {
        juce::Path l;
        l.startNewSubPath (dragStart);
        l.lineTo (lastPos);
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (l, l, dashes, 2);
        g.setColour (Colours::accent);
        g.fillPath (l);
    }

    g.restoreState();
}

//==============================================================================
void PianoRoll::pushUndo()
{
    undoStack.push_back (tl.copy (2));
    if (undoStack.size() > 40)
        undoStack.erase (undoStack.begin());
}

void PianoRoll::undo()
{
    if (undoStack.empty()) return;
    auto snapshot = std::move (undoStack.back());
    undoStack.pop_back();
    tl.clearTarget();
    tl.restore (2, snapshot.data(), (int) snapshot.size());
    if (onEdit) onEdit();
    repaint();
}

void PianoRoll::writeSegment (const PitchTimeline::Segment& seg, float newCentre, bool)
{
    const float var = variation();
    for (int f = seg.start; f < seg.end; ++f)
    {
        const float in = tl.input (f);
        tl.setTarget (f, in > 0.0f ? newCentre + (in - seg.centre) * var : 0.0f);
    }
}

void PianoRoll::writeLine (int f0, float m0, int f1, float m1)
{
    if (f1 < f0) { std::swap (f0, f1); std::swap (m0, m1); }
    for (int f = f0; f <= f1; ++f)
    {
        const float t = f1 == f0 ? 0.0f : (float) (f - f0) / (float) (f1 - f0);
        tl.setTarget (f, juce::jlimit (minMidi, maxMidi, m0 + (m1 - m0) * t));
    }
}

void PianoRoll::eraseRange (int f0, int f1)
{
    if (f1 < f0) std::swap (f0, f1);
    for (int f = juce::jmax (0, f0); f <= f1; ++f)
        tl.setTarget (f, 0.0f);
}

void PianoRoll::snapAllNotes()
{
    pushUndo();
    const uint32_t mask = proc.currentScaleMask();
    for (const auto& s : tl.segments (0, tl.usedFrames()))
        writeSegment (s, (float) s77::nearestAllowed (s.centre, mask), true);
    if (onEdit) onEdit();
    repaint();
}

void PianoRoll::drawFromReference()
{
    pushUndo();
    const uint32_t mask = proc.currentScaleMask();
    const bool snap = proc.apvts.getRawParameterValue (ParamIDs::refSnap)->load() > 0.5f;
    const int used = tl.usedFrames();
    for (int f = 0; f < used; ++f)
    {
        const float r = tl.ref (f), in = tl.input (f);
        if (r > 0.0f && in > 0.0f)
        {
            float t = r + 12.0f * std::round ((in - r) / 12.0f);
            if (snap) t = (float) s77::nearestAllowed (t, mask);
            tl.setTarget (f, t);
        }
    }
    if (onEdit) onEdit();
    repaint();
}

void PianoRoll::clearTarget()
{
    pushUndo();
    tl.clearTarget();
    hasSelection = false;
    if (onEdit) onEdit();
    repaint();
}

void PianoRoll::zoomToFit()
{
    const int used = tl.usedFrames();
    int first = -1, last = -1;
    float lo = 200.0f, hi = 0.0f;
    for (int f = 0; f < used; ++f)
    {
        const float v = juce::jmax (tl.input (f), tl.target (f));
        if (v <= 0.0f) continue;
        if (first < 0) first = f;
        last = f;
        lo = juce::jmin (lo, tl.input (f) > 0.0f ? tl.input (f) : v);
        hi = juce::jmax (hi, v);
    }
    if (first < 0) return;

    const auto grid = gridArea();
    const double dur = juce::jmax (1.0, (double) (last - first) / PitchTimeline::fps);
    pxPerSec = grid.getWidth() / (dur * 1.06);
    viewStart = juce::jmax (0.0, (double) first / PitchTimeline::fps - dur * 0.03);
    const float range = juce::jmax (12.0f, hi - lo + 6.0f);
    rowHeight = (float) grid.getHeight() / range;
    topMidi = hi + 3.0f;
    clampView();
    repaint();
}

//==============================================================================
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto pos = e.position;
    dragStart = lastPos = pos;
    const auto grid = gridArea();
    if (! grid.contains (pos.toInt()))
        return;

    if (e.mods.isMiddleButtonDown())
    {
        drag = Drag::Pan; panStartTime = viewStart; panStartMidi = topMidi;
        return;
    }

    const int f = frameForX (pos.x);
    const float m = midiForY (pos.y);

    if (e.mods.isPopupMenu())
    {
        PitchTimeline::Segment seg;
        const bool onNote = tl.segmentAt (f, seg);
        juce::PopupMenu menu;
        menu.addItem (1, "Snap note to scale", onNote);
        menu.addItem (2, "Reset note", onNote);
        menu.addSeparator();
        menu.addItem (3, "Snap all notes");
        menu.addItem (4, "Draw from reference");
        menu.addItem (5, "Clear all corrections");
        menu.addSeparator();
        menu.addItem (6, "Zoom to fit");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
            [this, seg, onNote] (int r)
            {
                const uint32_t mask = proc.currentScaleMask();
                if (r == 1 && onNote) { pushUndo(); writeSegment (seg, (float) s77::nearestAllowed (seg.centre, mask), true); }
                if (r == 2 && onNote) { pushUndo(); eraseRange (seg.start, seg.end - 1); }
                if (r == 3) snapAllNotes();
                if (r == 4) drawFromReference();
                if (r == 5) clearTarget();
                if (r == 6) zoomToFit();
                if (onEdit) onEdit();
                repaint();
            });
        return;
    }

    switch (tool)
    {
        case Tool::Select:
        {
            PitchTimeline::Segment seg;
            if (tl.segmentAt (f, seg))
            {
                const float in = tl.input (juce::jlimit (seg.start, seg.end - 1, f));
                const float tg = tl.target (f);
                const bool hit = std::abs (m - in) < 1.3f || (tg > 0.0f && std::abs (m - tg) < 1.3f)
                              || std::abs (m - seg.centre) < 1.3f;
                if (hit)
                {
                    pushUndo();
                    drag = Drag::Note;
                    dragSeg = seg;
                    selected = seg;
                    hasSelection = true;

                    // current offset of this note relative to its snapped centre
                    std::vector<float> diffs;
                    for (int i = seg.start; i < seg.end; ++i)
                        if (tl.target (i) > 0.0f && tl.input (i) > 0.0f)
                            diffs.push_back (tl.target (i) - tl.input (i));
                    const float base = std::round (seg.centre);
                    if (! diffs.empty())
                    {
                        std::nth_element (diffs.begin(), diffs.begin() + (long) diffs.size() / 2, diffs.end());
                        dragBaseOffset = seg.centre + diffs[diffs.size() / 2] - base;
                    }
                    else
                        dragBaseOffset = 0.0f;
                    lastAppliedOffset = std::numeric_limits<float>::quiet_NaN();
                    repaint();
                    return;
                }
            }
            hasSelection = false;
            drag = Drag::Pan; panStartTime = viewStart; panStartMidi = topMidi;
            repaint();
            break;
        }
        case Tool::Pencil:
        {
            pushUndo();
            drag = Drag::Pencil;
            const float v = e.mods.isShiftDown() ? std::round (m) : m;
            tl.setTarget (f, v);
            repaint();
            break;
        }
        case Tool::Line:
            pushUndo();
            drag = Drag::Line;
            break;
        case Tool::Erase:
            pushUndo();
            drag = Drag::Erase;
            eraseRange (f - 2, f + 2);
            repaint();
            break;
    }
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto pos = e.position;

    switch (drag)
    {
        case Drag::Pan:
        {
            const auto delta = pos - dragStart;
            viewStart = panStartTime - delta.x / pxPerSec;
            topMidi = panStartMidi + delta.y / rowHeight;
            clampView();
            break;
        }
        case Drag::Note:
        {
            const float dy = (dragStart.y - pos.y) / rowHeight;
            float offset = dragBaseOffset + (e.mods.isAltDown() ? dy : std::round (dy));
            if (! e.mods.isAltDown())
                offset = std::round (offset);
            if (std::isnan (lastAppliedOffset) || std::abs (offset - lastAppliedOffset) > 1.0e-4f)
            {
                writeSegment (dragSeg, std::round (dragSeg.centre) + offset, true);
                lastAppliedOffset = offset;
            }
            break;
        }
        case Drag::Pencil:
        {
            const float m0 = midiForY (lastPos.y), m1 = midiForY (pos.y);
            const bool snap = e.mods.isShiftDown();
            writeLine (frameForX (lastPos.x), snap ? std::round (m0) : m0, frameForX (pos.x), snap ? std::round (m1) : m1);
            break;
        }
        case Drag::Erase:
            eraseRange (frameForX (juce::jmin (lastPos.x, pos.x)) - 2, frameForX (juce::jmax (lastPos.x, pos.x)) + 2);
            break;
        case Drag::Line:
        case Drag::None:
            break;
    }

    lastPos = pos;
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent& e)
{
    if (drag == Drag::Line)
    {
        float m0 = midiForY (dragStart.y), m1 = midiForY (e.position.y);
        if (e.mods.isShiftDown()) { m0 = std::round (m0); m1 = std::round (m1); }
        writeLine (frameForX (dragStart.x), m0, frameForX (e.position.x), m1);
    }

    if (drag != Drag::None && drag != Drag::Pan && onEdit)
        onEdit();

    drag = Drag::None;
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (tool != Tool::Select || ! gridArea().contains (e.position.toInt()))
        return;

    PitchTimeline::Segment seg;
    if (tl.segmentAt (frameForX (e.position.x), seg))
    {
        pushUndo();
        writeSegment (seg, (float) s77::nearestAllowed (seg.centre, proc.currentScaleMask()), true);
        selected = seg;
        hasSelection = true;
        if (onEdit) onEdit();
        repaint();
    }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const bool horizontal = std::abs (w.deltaX) > std::abs (w.deltaY);
    const float dy = horizontal ? w.deltaX : w.deltaY;

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = timeForX (e.position.x);
        pxPerSec *= std::pow (2.0, (double) dy * 2.0);
        clampView();
        viewStart = anchor - (e.position.x - (float) gridArea().getX()) / pxPerSec;
    }
    else if (e.mods.isAltDown())
    {
        const float anchor = midiForY (e.position.y);
        rowHeight *= std::pow (2.0f, dy * 1.5f);
        clampView();
        topMidi = anchor + (e.position.y - (float) gridArea().getY()) / rowHeight;
    }
    else if (e.mods.isShiftDown() || horizontal)
    {
        viewStart -= (double) dy * (gridArea().getWidth() / pxPerSec) * 0.35;
    }
    else
    {
        topMidi += dy * 8.0f;
    }

    clampView();
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { undo(); return true; }
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && hasSelection)
    {
        pushUndo();
        eraseRange (selected.start, selected.end - 1);
        if (onEdit) onEdit();
        repaint();
        return true;
    }
    if (k.getTextCharacter() == '1') { setTool (Tool::Select); if (onEdit) onEdit(); return true; }
    if (k.getTextCharacter() == '2') { setTool (Tool::Pencil); if (onEdit) onEdit(); return true; }
    if (k.getTextCharacter() == '3') { setTool (Tool::Line);   if (onEdit) onEdit(); return true; }
    if (k.getTextCharacter() == '4') { setTool (Tool::Erase);  if (onEdit) onEdit(); return true; }
    return false;
}

} // namespace s77::ui
