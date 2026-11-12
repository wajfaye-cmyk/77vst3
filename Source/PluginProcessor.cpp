#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace s77;

//==============================================================================
SeventySevenProcessor::SeventySevenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMode       = apvts.getRawParameterValue (ParamIDs::mode);
    pKey        = apvts.getRawParameterValue (ParamIDs::key);
    pScale      = apvts.getRawParameterValue (ParamIDs::scale);
    pRetune     = apvts.getRawParameterValue (ParamIDs::retune);
    pHumanize   = apvts.getRawParameterValue (ParamIDs::humanize);
    pMix        = apvts.getRawParameterValue (ParamIDs::mix);
    pOutput     = apvts.getRawParameterValue (ParamIDs::output);
    pDrawAmount = apvts.getRawParameterValue (ParamIDs::drawAmount);
    pDrawSmooth = apvts.getRawParameterValue (ParamIDs::drawSmooth);
    pAutoDraw   = apvts.getRawParameterValue (ParamIDs::autoDraw);
    pRefSnap    = apvts.getRawParameterValue (ParamIDs::refSnap);

    for (int i = 0; i < Telemetry::graphSize; ++i)
    {
        telemetry.graphIn[i].store (0.0f);
        telemetry.graphOut[i].store (0.0f);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SeventySevenProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto choices = [] (const char* const* names)
    {
        StringArray a;
        for (int i = 0; names[i] != nullptr; ++i) a.add (names[i]);
        return a;
    };

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });
    auto db  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Engine", StringArray { "Vocal Draw", "Auto-Tune" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::key, 1 }, "Key", choices (keyNames()), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::scale, 1 }, "Scale", choices (scaleNames()), 1));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::retune, 1 }, "Retune Speed",
                                                       NormalisableRange<float> (0.0f, 400.0f, 1.0f, 0.45f), 20.0f, ms));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::humanize, 1 }, "Humanize",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drawAmount, 1 }, "Correction",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drawSmooth, 1 }, "Transition",
                                                       NormalisableRange<float> (0.0f, 250.0f, 1.0f, 0.5f), 25.0f, ms));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::variation, 1 }, "Pitch Variation",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f, pct));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoDraw, 1 }, "Auto Draw", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::refSnap, 1 }, "Snap Reference", false));

    return layout;
}

bool SeventySevenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

uint32_t SeventySevenProcessor::currentScaleMask() const
{
    const auto scale = (ScaleType) juce::roundToInt (pScale->load());
    if (scale == ScaleType::Custom)
        return customMask.load();
    return scaleMask (juce::roundToInt (pKey->load()), scale);
}

//==============================================================================
void SeventySevenProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    mainDetector.prepare (sr);
    refDetector.prepare (sr);

    const int channels = juce::jmax (1, getMainBusNumOutputChannels());
    shifter.prepare (channels, mainDetector.maxPeriodSamples(), mainDetector.maxLagSamples() + 32);
    setLatencySamples (shifter.latency());

    frameIn.allocate ((size_t) channels, true);
    frameWet.allocate ((size_t) channels, true);
    frameDry.allocate ((size_t) channels, true);

    unvoicedPeriod = juce::roundToInt (sr / 180.0);
    engageStep = (float) (1.0 / (0.025 * sr));

    mainHistory.reset();
    refHistory.reset();
    currentShift = 0.0f; slowMidi = 0.0f; heldNote = -1; lastMark = 0; wasVoiced = false; engage = 0.0f;
    refRms = 0.0f;
    hostOffsetValid = false;

    mixSmoothed.reset (sr, 0.05);
    gainSmoothed.reset (sr, 0.05);
    mixSmoothed.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    telemetry.latencySamples.store (shifter.latency());
    telemetry.sampleRate.store (sr);
}

int SeventySevenProcessor::frameForSample (int64_t absSample) const noexcept
{
    const double t = (double) (absSample + hostOffset) / sr;
    return (int) std::floor (t * PitchTimeline::fps);
}

float SeventySevenProcessor::autoDrawTarget (float refMidi, float voiceMidi, uint32_t mask) const
{
    // follow the reference melody, transposed to the octave the singer is in
    float r = refMidi + 12.0f * std::round ((voiceMidi - refMidi) / 12.0f);
    if (pRefSnap->load() > 0.5f)
        r = (float) nearestAllowed (r, mask);
    return r;
}

float SeventySevenProcessor::computeShift (int64_t mark, const HistoryEntry& voice, EngineMode mode, uint32_t mask)
{
    const float dt = (float) juce::jmax<int64_t> (0, mark - lastMark) / (float) sr;
    lastMark = mark;

    if (! voice.voiced)
    {
        wasVoiced = false;
        heldNote = -1;
        currentShift += (0.0f - currentShift) * (1.0f - std::exp (-dt / 0.03f));
        return currentShift;
    }

    if (! wasVoiced)
        slowMidi = voice.midi;
    slowMidi += (voice.midi - slowMidi) * (1.0f - std::exp (-dt / 0.25f));

    float desired = 0.0f, tauMs = 0.0f;

    if (mode == EngineMode::AutoTune)
    {
        int note = nearestAllowed (voice.midi, mask);
        if (heldNote >= 0 && (mask & (1u << pitchClass (heldNote))) != 0 && std::abs (voice.midi - (float) heldNote) < 0.62f)
            note = heldNote;   // hysteresis: avoid flipping between neighbours
        heldNote = note;

        const float humanize = pHumanize->load() * 0.01f;
        const float target = (float) note + humanize * (voice.midi - slowMidi);
        desired = target - voice.midi;
        tauMs = pRetune->load();
    }
    else
    {
        float target = 0.0f;
        if (hostPlaying && hostOffsetValid)
        {
            const double tf = (double) (mark + hostOffset) / sr * PitchTimeline::fps;
            const int f0 = (int) std::floor (tf);
            const float t0 = timeline.target (f0), t1 = timeline.target (f0 + 1);
            const float frac = (float) (tf - f0);
            target = (t0 > 0.0f && t1 > 0.0f) ? t0 + (t1 - t0) * frac : t0;
        }

        if (target <= 0.0f && pAutoDraw->load() > 0.5f && telemetry.sidechainSignal.load (std::memory_order_relaxed))
        {
            const auto ref = refHistory.lookup (mark);
            if (ref.voiced)
                target = autoDrawTarget (ref.midi, voice.midi, mask);
        }

        if (target > 0.0f)
            desired = (target - voice.midi) * (pDrawAmount->load() * 0.01f);
        tauMs = pDrawSmooth->load();
    }

    desired = juce::jlimit (-12.0f, 12.0f, desired);

    if (! wasVoiced)
        currentShift = desired;   // note onset: land on pitch immediately
    else
    {
        const float a = tauMs <= 0.5f ? 1.0f : 1.0f - std::exp (-dt * 1000.0f / tauMs);
        currentShift += (desired - currentShift) * a;
    }

    wasVoiced = true;
    return currentShift;
}

//==============================================================================
void SeventySevenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, false, 0);
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (mainBus.getNumChannels(), shifter.numChannels());

    // ---- side-chain ---------------------------------------------------------
    const float* scData[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1)
    {
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            scCh = juce::jmin (2, sc.getNumChannels());
            for (int c = 0; c < scCh; ++c)
                scData[c] = sc.getReadPointer (c);

            float rms = 0.0f;
            for (int c = 0; c < scCh; ++c)
                rms = juce::jmax (rms, sc.getRMSLevel (c, 0, numSamples));
            refRms = refRms * 0.9f + rms * 0.1f;
        }
    }
    const bool scConnected = scCh > 0;
    const bool scSignal = scConnected && refRms > 1.0e-4f;
    telemetry.sidechainConnected.store (scConnected, std::memory_order_relaxed);
    telemetry.sidechainSignal.store (scSignal, std::memory_order_relaxed);

    // ---- host timeline -----------------------------------------------------
    hostPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying() || pos->getIsRecording();
            if (auto samples = pos->getTimeInSamples())
            {
                hostOffset = *samples - shifter.samplesWritten();
                hostOffsetValid = true;
            }
            else if (auto secs = pos->getTimeInSeconds())
            {
                hostOffset = (int64_t) std::llround (*secs * sr) - shifter.samplesWritten();
                hostOffsetValid = true;
            }
            if (hostOffsetValid)
                telemetry.playheadSeconds.store ((double) (shifter.samplesWritten() + hostOffset) / sr, std::memory_order_relaxed);
        }
    }
    telemetry.playing.store (hostPlaying, std::memory_order_relaxed);

    const auto mode = (EngineMode) juce::roundToInt (pMode->load());
    const uint32_t mask = currentScaleMask();
    const bool capture = hostPlaying && hostOffsetValid && mode == EngineMode::VocalDraw;
    const bool autoDraw = pAutoDraw->load() > 0.5f;

    mixSmoothed.setTargetValue (pMix->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    for (int i = 0; i < numSamples; ++i)
    {
        const int64_t now = shifter.samplesWritten();

        // reference vocal analysis
        if (scConnected)
        {
            float x = 0.0f;
            for (int c = 0; c < scCh; ++c) x += scData[c][i];
            x /= (float) scCh;

            if (refDetector.push (x))
            {
                const auto& e = refDetector.latest();
                refHistory.add ({ now - e.lagSamples, e.midi, e.periodSamples, e.voiced });
                telemetry.refMidi.store (e.voiced ? e.midi : 0.0f, std::memory_order_relaxed);
            }
        }

        // main vocal analysis
        float mono = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            frameIn[c] = mainBus.getSample (c, i);
            mono += frameIn[c];
        }
        mono /= (float) juce::jmax (1, numCh);

        if (mainDetector.push (mono))
        {
            const auto& e = mainDetector.latest();
            const int64_t centre = now - e.lagSamples;
            mainHistory.add ({ centre, e.midi, e.periodSamples, e.voiced });

            const float inMidi = e.voiced ? e.midi : 0.0f;
            const float outMidi = e.voiced ? e.midi + currentShift : 0.0f;
            telemetry.inputMidi.store (inMidi, std::memory_order_relaxed);
            telemetry.outputMidi.store (outMidi, std::memory_order_relaxed);
            telemetry.shiftSemis.store (currentShift, std::memory_order_relaxed);

            const int gw = telemetry.graphWrite.load (std::memory_order_relaxed);
            telemetry.graphIn[gw].store (inMidi, std::memory_order_relaxed);
            telemetry.graphOut[gw].store (outMidi, std::memory_order_relaxed);
            telemetry.graphWrite.store ((gw + 1) % Telemetry::graphSize, std::memory_order_release);

            if (capture)
            {
                const int f = frameForSample (centre);
                timeline.setInput (f, inMidi);

                if (scConnected)
                {
                    const auto ref = refHistory.lookup (centre);
                    timeline.setRef (f, ref.voiced ? ref.midi : 0.0f);

                    if (autoDraw && scSignal && ref.voiced && e.voiced)
                        timeline.setTarget (f, autoDrawTarget (ref.midi, e.midi, mask));
                }
            }
        }

        // pitch shifting
        shifter.push (frameIn.getData());

        while (shifter.needsGrain())
        {
            const auto mark = shifter.nextMark();
            const auto voice = mainHistory.lookup (mark);
            const float shift = computeShift (mark, voice, mode, mask);

            if (voice.voiced)
                shifter.emitGrain (voice.period, std::pow (2.0f, shift / 12.0f));
            else
                shifter.emitGrain ((float) unvoicedPeriod, 1.0f);
        }

        shifter.pop (frameWet.getData(), frameDry.getData());

        // engage the shifter only while it is actually correcting (fully transparent otherwise)
        const bool active = std::abs (currentShift) > 0.015f;
        engage = active ? juce::jmin (1.0f, engage + engageStep) : juce::jmax (0.0f, engage - engageStep);
        if (engage <= 0.0f)
            shifter.requestResync();

        const float wet = mixSmoothed.getNextValue() * engage;
        const float gain = gainSmoothed.getNextValue();

        for (int c = 0; c < numCh; ++c)
            mainBus.setSample (c, i, (frameDry[c] + (frameWet[c] - frameDry[c]) * wet) * gain);
    }

    for (int c = numCh; c < mainBus.getNumChannels(); ++c)
        mainBus.clear (c, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* SeventySevenProcessor::createEditor()
{
    return new SeventySevenEditor (*this);
}

void SeventySevenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("customMask", (int) customMask.load(), nullptr);

    juce::ValueTree tl ("TIMELINE");
    const int n = timeline.usedFrames();
    tl.setProperty ("frames", n, nullptr);
    const char* names[] = { "input", "ref", "target" };
    for (int w = 0; w < 3; ++w)
    {
        const auto data = timeline.copy (w);
        tl.setProperty (names[w], juce::var (juce::MemoryBlock (data.data(), data.size() * sizeof (float))), nullptr);
    }
    state.appendChild (tl, nullptr);

    juce::MemoryOutputStream stream (destData, false);
    stream.writeString ("77MP3_STATE_V1");
    state.writeToStream (stream);
}

void SeventySevenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, (size_t) sizeInBytes, false);
    if (stream.readString() != "77MP3_STATE_V1")
        return;

    auto state = juce::ValueTree::readFromStream (stream);
    if (! state.isValid())
        return;

    customMask.store ((uint32_t) (int) state.getProperty ("customMask", 0xAB5));

    if (auto tl = state.getChildWithName ("TIMELINE"); tl.isValid())
    {
        timeline.clearAll();
        const int frames = tl.getProperty ("frames", 0);
        const char* names[] = { "input", "ref", "target" };
        for (int w = 0; w < 3; ++w)
            if (auto* mb = tl.getProperty (names[w]).getBinaryData())
                timeline.restore (w, static_cast<const float*> (mb->getData()),
                                  juce::jmin (frames, (int) (mb->getSize() / sizeof (float))));
        state.removeChild (tl, nullptr);
    }

    state.removeProperty ("customMask", nullptr);
    if (state.hasType (apvts.state.getType()))
        apvts.replaceState (state);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeventySevenProcessor();
}
