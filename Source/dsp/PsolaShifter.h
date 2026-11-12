#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace s77
{

/**
    Real-time TD-PSOLA pitch shifter.

    Grains of two input periods are cut at analysis marks spaced exactly one period
    apart and re-placed at synthesis marks spaced period / ratio apart. Because the
    analysis marks follow the detected period, consecutive grains stay phase-coherent,
    which keeps the voice natural and preserves the formants (no "chipmunk" effect).

    Timing model (all indices are absolute sample counters):
      - input sample n is written at push()
      - grains are scheduled for synthesis mark s when s <= n - lookahead
      - output is read at n - lookahead - maxPeriod
    Total latency = lookahead + maxPeriod, reported to the host.
*/
class PsolaShifter
{
public:
    void prepare (int numChannels, int maxPeriodSamples, int lookaheadSamples)
    {
        channels = std::max (1, numChannels);
        P = std::max (32, maxPeriodSamples);
        L = std::max ((P * 3) / 2 + 2, lookaheadSamples);

        int size = 1;
        while (size < L + P * 4 + 8192)
            size <<= 1;
        mask = size - 1;

        in.assign ((size_t) channels, std::vector<float> ((size_t) size, 0.0f));
        acc.assign ((size_t) channels, std::vector<float> ((size_t) size, 0.0f));

        buildWindowTable();
        reset();
    }

    void reset()
    {
        for (auto& c : in)  std::fill (c.begin(), c.end(), 0.0f);
        for (auto& c : acc) std::fill (c.begin(), c.end(), 0.0f);
        n = 0;
        synthPos = 0.0;
        anaMark = 0;
        resyncPending = true;
    }

    int latency() const noexcept { return L + P; }
    int numChannels() const noexcept { return channels; }

    /** Writes one multi-channel input frame. */
    void push (const float* frameIn) noexcept
    {
        const auto idx = (size_t) (n & mask);
        for (int c = 0; c < channels; ++c)
            in[(size_t) c][idx] = frameIn[c];
        ++n;
    }

    bool needsGrain() const noexcept     { return (int64_t) synthPos <= (n - 1) - L; }
    int64_t nextMark() const noexcept    { return (int64_t) synthPos; }

    /** Re-aligns analysis and synthesis marks (call while the effect is disengaged). */
    void requestResync() noexcept { resyncPending = true; }

    /**
        Emits the grain for the pending synthesis mark.
        @param period  current input period in samples (use a fixed value for unvoiced audio)
        @param ratio   output / input frequency ratio
    */
    void emitGrain (float period, float ratio) noexcept
    {
        const int T = std::clamp ((int) std::lround (period), 16, P);
        ratio = std::clamp (ratio, 0.5f, 2.0f);
        const int64_t s = (int64_t) synthPos;

        if (resyncPending || anaMark < s - 2 * T || anaMark > s + 2 * T)
        {
            anaMark = s;
            resyncPending = false;
        }

        while (anaMark + T / 2 < s) anaMark += T;
        while (anaMark - T / 2 > s) anaMark -= T;

        const float gain = ratio > 1.0f ? 1.0f / ratio : 1.0f;
        const int len = 2 * T;
        const double step = (double) windowTable.size() / (double) len;

        for (int c = 0; c < channels; ++c)
        {
            const float* src = in[(size_t) c].data();
            float* dst = acc[(size_t) c].data();
            int64_t iIn = anaMark - T, iOut = s - T;
            double wPos = 0.0;

            for (int k = 0; k < len; ++k, ++iIn, ++iOut, wPos += step)
                dst[iOut & mask] += src[iIn & mask] * windowTable[(size_t) wPos] * gain;
        }

        synthPos += std::max (1.0, (double) T / (double) ratio);
    }

    /** Reads one output frame (wet) and the latency-aligned dry signal. */
    void pop (float* wetOut, float* dryOut) noexcept
    {
        const int64_t o = (n - 1) - L - P;
        const auto idx = (size_t) (o & mask);
        for (int c = 0; c < channels; ++c)
        {
            wetOut[c] = acc[(size_t) c][idx];
            dryOut[c] = o >= 0 ? in[(size_t) c][idx] : 0.0f;
            acc[(size_t) c][idx] = 0.0f;
        }
    }

    int64_t samplesWritten() const noexcept { return n; }

private:
    void buildWindowTable()
    {
        // periodic Hann: overlapping copies at half-length hop sum to exactly 1
        windowTable.resize (4096);
        for (size_t i = 0; i < windowTable.size(); ++i)
            windowTable[i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265358979f * (float) i / (float) windowTable.size());
    }

    int channels = 2, P = 800, L = 1200;
    int64_t mask = 0;
    std::vector<std::vector<float>> in, acc;
    std::vector<float> windowTable;

    int64_t n = 0;
    double synthPos = 0.0;
    int64_t anaMark = 0;
    bool resyncPending = true;
};

} // namespace s77
