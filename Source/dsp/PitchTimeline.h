#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace s77
{

/**
    Pitch curves stored against the host timeline at 100 frames per second.

      input  : pitch detected on the vocal this plugin is inserted on
      ref    : pitch detected on the side-chained reference vocal
      target : the drawn / generated correction curve (0 = no correction)

    Values are fractional MIDI notes, 0 means "no pitch". Arrays are lock-free:
    the audio thread captures input/ref and reads target, the editor edits target.
*/
class PitchTimeline
{
public:
    static constexpr int fps = 100;
    static constexpr int maxFrames = fps * 60 * 30; // 30 minutes

    PitchTimeline()
        : inputCurve (new std::atomic<float>[maxFrames]),
          refCurve (new std::atomic<float>[maxFrames]),
          targetCurve (new std::atomic<float>[maxFrames])
    {
        clearAll();
    }

    void clearAll()
    {
        for (int i = 0; i < maxFrames; ++i)
        {
            inputCurve[(size_t) i].store (0.0f, std::memory_order_relaxed);
            refCurve[(size_t) i].store (0.0f, std::memory_order_relaxed);
            targetCurve[(size_t) i].store (0.0f, std::memory_order_relaxed);
        }
        used.store (0);
    }

    void clearTarget()
    {
        for (int i = 0; i < maxFrames; ++i)
            targetCurve[(size_t) i].store (0.0f, std::memory_order_relaxed);
    }

    static bool valid (int f) noexcept { return f >= 0 && f < maxFrames; }

    float input (int f) const noexcept  { return valid (f) ? inputCurve[(size_t) f].load (std::memory_order_relaxed) : 0.0f; }
    float ref (int f) const noexcept    { return valid (f) ? refCurve[(size_t) f].load (std::memory_order_relaxed) : 0.0f; }
    float target (int f) const noexcept { return valid (f) ? targetCurve[(size_t) f].load (std::memory_order_relaxed) : 0.0f; }

    void setInput (int f, float v) noexcept  { if (valid (f)) { inputCurve[(size_t) f].store (v, std::memory_order_relaxed); markUsed (f); } }
    void setRef (int f, float v) noexcept    { if (valid (f)) { refCurve[(size_t) f].store (v, std::memory_order_relaxed); markUsed (f); } }
    void setTarget (int f, float v) noexcept { if (valid (f)) { targetCurve[(size_t) f].store (v, std::memory_order_relaxed); markUsed (f); } }

    int usedFrames() const noexcept { return used.load (std::memory_order_relaxed); }

    // ---- bulk access (message thread) --------------------------------------
    std::vector<float> copy (int which) const
    {
        const int n = usedFrames();
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = which == 0 ? input (i) : which == 1 ? ref (i) : target (i);
        return v;
    }

    void restore (int which, const float* data, int n)
    {
        n = std::min (n, maxFrames);
        for (int i = 0; i < n; ++i)
        {
            auto& arr = which == 0 ? inputCurve : which == 1 ? refCurve : targetCurve;
            arr[(size_t) i].store (data[i], std::memory_order_relaxed);
        }
        if (n > 0) markUsed (n - 1);
    }

    // ---- note segmentation ---------------------------------------------------
    struct Segment { int start = 0, end = 0; float centre = 0.0f; };

    /** Splits the detected input curve into note "blobs" (like Newtone). */
    std::vector<Segment> segments (int from, int to) const
    {
        std::vector<Segment> out;
        from = std::max (0, from);
        to = std::min (to, usedFrames());

        int f = from;
        while (f < to)
        {
            if (input (f) <= 0.0f) { ++f; continue; }

            Segment s;
            s.start = f;
            float prev = input (f);
            int gap = 0, last = f;
            ++f;
            while (f < to)
            {
                const float v = input (f);
                if (v <= 0.0f)
                {
                    if (++gap > 3) break;
                    ++f; continue;
                }
                if (std::abs (v - prev) > 0.85f) break;
                prev = v; gap = 0; last = f; ++f;
            }
            s.end = last + 1;
            if (s.end - s.start >= 4)
            {
                s.centre = median (s.start, s.end);
                out.push_back (s);
            }
            f = last + 1;
        }
        return out;
    }

    /** Finds the note segment around frame f (scans outward, not limited to a view). */
    bool segmentAt (int f, Segment& result) const
    {
        if (input (f) <= 0.0f)
        {
            // allow clicking inside a tiny gap
            int g = 1;
            while (g <= 3 && input (f - g) <= 0.0f && input (f + g) <= 0.0f) ++g;
            if (g > 3) return false;
            f = input (f - g) > 0.0f ? f - g : f + g;
        }
        int a = f;
        while (a > 0 && input (a - 1) > 0.0f && std::abs (input (a - 1) - input (a)) <= 0.85f) --a;
        int b = f;
        while (b + 1 < usedFrames() && input (b + 1) > 0.0f && std::abs (input (b + 1) - input (b)) <= 0.85f) ++b;
        if (b - a + 1 < 2) return false;
        result.start = a;
        result.end = b + 1;
        result.centre = median (a, b + 1);
        return true;
    }

    float median (int a, int b) const
    {
        std::vector<float> v;
        v.reserve ((size_t) std::max (0, b - a));
        for (int i = a; i < b; ++i)
            if (input (i) > 0.0f) v.push_back (input (i));
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    }

private:
    void markUsed (int f) noexcept
    {
        int cur = used.load (std::memory_order_relaxed);
        while (f + 1 > cur && ! used.compare_exchange_weak (cur, f + 1, std::memory_order_relaxed)) {}
    }

    std::unique_ptr<std::atomic<float>[]> inputCurve, refCurve, targetCurve;
    std::atomic<int> used { 0 };
};

} // namespace s77
