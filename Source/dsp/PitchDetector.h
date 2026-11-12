#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace s77
{

struct PitchEstimate
{
    bool  voiced        = false;
    float midi          = 0.0f;   // fractional MIDI note (69 = A4)
    float hz            = 0.0f;
    float periodSamples = 0.0f;   // period at the host sample rate
    float confidence    = 0.0f;   // 0..1
    int   lagSamples    = 0;      // distance from "now" to the centre of the analysed window
    float rmsDb         = -120.0f;
};

/**
    Real-time monophonic pitch detector (YIN, de Cheveigné & Kawahara 2002).

    The input is low-passed and decimated to ~11-12 kHz before analysis, which keeps
    the cost low enough to run continuously on the audio thread while still covering
    the full vocal range (60 Hz - 1.1 kHz).
*/
class PitchDetector
{
public:
    void prepare (double sampleRate, double minHz = 60.0, double maxHz = 1100.0)
    {
        sr     = sampleRate;
        decim  = std::max (1, (int) std::floor (sr / 11000.0));
        dsr    = sr / (double) decim;
        W      = std::max (64, (int) std::round (dsr * 0.030));
        tauMin = std::max (2, (int) std::floor (dsr / maxHz));
        tauMax = (int) std::ceil (dsr / minHz);
        N      = W + tauMax + 2;
        hop    = std::max (16, (int) std::round (dsr * 0.004));

        ring.assign ((size_t) N, 0.0f);
        frame.assign ((size_t) N, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);

        designLowpass (std::min (0.36 * dsr, 4200.0));
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0; filled = 0; decCount = 0; hopCount = 0;
        for (auto& s : lp) s.z1 = s.z2 = 0.0f;
        lastEstimate = {};
        medianCount = 0;
    }

    /** Pushes one sample. Returns true when a new estimate is available in latest(). */
    bool push (float x) noexcept
    {
        float y = x;
        for (auto& s : lp)
            y = s.process (y);

        if (++decCount < decim)
            return false;

        decCount = 0;
        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) % N;
        if (filled < N) ++filled;

        if (++hopCount < hop)
            return false;

        hopCount = 0;
        if (filled < N)
            return false;

        analyse();
        return true;
    }

    const PitchEstimate& latest() const noexcept { return lastEstimate; }

    /** Worst-case analysis delay in host samples. */
    int maxLagSamples() const noexcept { return (W / 2 + tauMax / 2 + hop) * decim; }
    int maxPeriodSamples() const noexcept { return tauMax * decim; }
    int minPeriodSamples() const noexcept { return tauMin * decim; }

    void setSensitivity (float threshold) noexcept { yinThreshold = threshold; }
    void setGateDb (float db) noexcept { gateDb = db; }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void designLowpass (double cutoff)
    {
        static constexpr double qs[2] = { 0.54119610, 1.30656296 }; // 4th-order Butterworth
        for (int i = 0; i < 2; ++i)
        {
            const double w0 = 2.0 * 3.14159265358979323846 * cutoff / sr;
            const double alpha = std::sin (w0) / (2.0 * qs[i]);
            const double c = std::cos (w0);
            const double a0 = 1.0 + alpha;
            lp[(size_t) i].b0 = (float) (((1.0 - c) * 0.5) / a0);
            lp[(size_t) i].b1 = (float) ((1.0 - c) / a0);
            lp[(size_t) i].b2 = (float) (((1.0 - c) * 0.5) / a0);
            lp[(size_t) i].a1 = (float) ((-2.0 * c) / a0);
            lp[(size_t) i].a2 = (float) ((1.0 - alpha) / a0);
        }
    }

    void analyse() noexcept
    {
        // unroll ring buffer, oldest -> newest
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % N)];

        const float* ref = frame.data() + (N - W);   // newest W samples

        double energy = 0.0;
        for (int j = 0; j < W; ++j)
            energy += (double) ref[j] * ref[j];

        const float rms = (float) std::sqrt (energy / W);
        const float rmsDb = 20.0f * std::log10 (rms + 1.0e-9f);

        PitchEstimate est;
        est.rmsDb = rmsDb;

        if (rmsDb < gateDb)
        {
            est.lagSamples = (W / 2 + tauMax / 4) * decim;
            lastEstimate = est;
            medianCount = 0;
            return;
        }

        // difference function, comparing the newest window against earlier material
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float* past = ref - tau;
            double d = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const float delta = ref[j] - past[j];
                d += (double) delta * delta;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        // absolute threshold, then walk down to the local minimum
        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (diff[(size_t) tau] < yinThreshold)
            {
                while (tau + 1 <= tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            est.lagSamples = (W / 2 + tauMax / 4) * decim;
            lastEstimate = est;
            medianCount = 0;
            return;
        }

        // parabolic interpolation
        float tauF = (float) best;
        if (best > tauMin && best < tauMax)
        {
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float denom = s0 - 2.0f * s1 + s2;
            if (std::abs (denom) > 1.0e-9f)
                tauF += std::clamp (0.5f * (s0 - s2) / denom, -0.5f, 0.5f);
        }

        const float period = tauF * (float) decim;
        const float hz = (float) sr / period;
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);

        // 3-point median against octave blips
        medianBuf[(size_t) (medianPos++ % 3)] = midi;
        medianCount = std::min (medianCount + 1, 3);
        float smoothed = midi;
        if (medianCount == 3)
        {
            auto m = medianBuf;
            std::sort (m.begin(), m.end());
            smoothed = m[1];
        }

        est.voiced        = true;
        est.midi          = smoothed;
        est.hz            = 440.0f * std::pow (2.0f, (smoothed - 69.0f) / 12.0f);
        est.periodSamples = (float) sr / est.hz;
        est.confidence    = std::clamp (1.0f - diff[(size_t) best], 0.0f, 1.0f);
        est.lagSamples    = (int) ((W * 0.5f + tauF * 0.5f) * decim);
        lastEstimate = est;
    }

    double sr = 44100.0, dsr = 11025.0;
    int decim = 4, W = 330, tauMin = 10, tauMax = 184, N = 516, hop = 44;
    int writePos = 0, filled = 0, decCount = 0, hopCount = 0;
    float yinThreshold = 0.18f, gateDb = -52.0f;

    std::vector<float> ring, frame, diff;
    std::array<Biquad, 2> lp;

    std::array<float, 3> medianBuf {};
    int medianPos = 0, medianCount = 0;

    PitchEstimate lastEstimate;
};

} // namespace s77
