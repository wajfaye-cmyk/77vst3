#pragma once

#include <JuceHeader.h>
#include "dsp/PitchDetector.h"
#include "dsp/PsolaShifter.h"
#include "dsp/PitchTimeline.h"
#include "dsp/Scale.h"

namespace ParamIDs
{
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* key        = "key";
    inline constexpr const char* scale      = "scale";
    inline constexpr const char* retune     = "retune";
    inline constexpr const char* humanize   = "humanize";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline constexpr const char* drawAmount = "drawAmount";
    inline constexpr const char* drawSmooth = "drawSmooth";
    inline constexpr const char* variation  = "variation";
    inline constexpr const char* autoDraw   = "autoDraw";
    inline constexpr const char* refSnap    = "refSnap";
}

enum class EngineMode { VocalDraw = 0, AutoTune = 1 };

class SeventySevenProcessor : public juce::AudioProcessor
{
public:
    SeventySevenProcessor();
    ~SeventySevenProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "77.MP3"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- shared state ------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    s77::PitchTimeline timeline;

    uint32_t currentScaleMask() const;
    void setCustomMask (uint32_t m) { customMask.store (m & 0xFFFu); }
    uint32_t getCustomMask() const  { return customMask.load(); }

    // ---- telemetry for the editor -----------------------------------------
    struct Telemetry
    {
        std::atomic<float> inputMidi { 0.0f };    // 0 = unvoiced
        std::atomic<float> outputMidi { 0.0f };
        std::atomic<float> refMidi { 0.0f };
        std::atomic<float> shiftSemis { 0.0f };
        std::atomic<bool>  sidechainConnected { false };
        std::atomic<bool>  sidechainSignal { false };
        std::atomic<bool>  playing { false };
        std::atomic<double> playheadSeconds { 0.0 };
        std::atomic<int>   latencySamples { 0 };
        std::atomic<double> sampleRate { 44100.0 };

        static constexpr int graphSize = 1024;
        std::atomic<float> graphIn[graphSize];
        std::atomic<float> graphOut[graphSize];
        std::atomic<int>   graphWrite { 0 };
    } telemetry;

    static constexpr const char* instagramUrl = "https://www.instagram.com/wajfay";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct HistoryEntry { int64_t centre = 0; float midi = 0.0f; float period = 0.0f; bool voiced = false; };

    struct History
    {
        static constexpr int size = 256;
        std::array<HistoryEntry, size> entries {};
        int write = 0, count = 0;

        void reset() { write = 0; count = 0; }
        void add (const HistoryEntry& e) { entries[(size_t) write] = e; write = (write + 1) % size; count = std::min (count + 1, size); }

        HistoryEntry lookup (int64_t s) const
        {
            if (count == 0) return {};
            int idx = (write - 1 + size) % size;
            HistoryEntry best = entries[(size_t) idx];
            for (int i = 0; i < count; ++i)
            {
                const auto& e = entries[(size_t) idx];
                best = e;
                if (e.centre <= s) break;
                idx = (idx - 1 + size) % size;
            }
            return best;
        }
    };

    float computeShift (int64_t mark, const HistoryEntry& voice, EngineMode mode, uint32_t mask);
    float autoDrawTarget (float refMidi, float voiceMidi, uint32_t mask) const;
    int frameForSample (int64_t absSample) const noexcept;

    // DSP
    s77::PitchDetector mainDetector, refDetector;
    s77::PsolaShifter shifter;
    History mainHistory, refHistory;

    double sr = 44100.0;
    int unvoicedPeriod = 220;

    float currentShift = 0.0f;
    float slowMidi = 0.0f;
    int   heldNote = -1;
    int64_t lastMark = 0;
    bool  wasVoiced = false;
    float engage = 0.0f;
    float engageStep = 0.001f;
    float refRms = 0.0f;

    // host timeline mapping
    bool    hostPlaying = false;
    int64_t hostOffset = 0;          // timelineSample = absSample + hostOffset
    bool    hostOffsetValid = false;

    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;
    juce::HeapBlock<float> frameIn, frameWet, frameDry;

    std::atomic<uint32_t> customMask { 0xAB5u };   // C major

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRetune = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pDrawAmount = nullptr;
    std::atomic<float>* pDrawSmooth = nullptr;
    std::atomic<float>* pAutoDraw = nullptr;
    std::atomic<float>* pRefSnap = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeventySevenProcessor)
};
