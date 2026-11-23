#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ab::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ab::monitor, 1 }, "Monitor",
        juce::StringArray { "A - Mix", "B - Reference" }, 0));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ab::refTrim, 1 }, "Reference Trim",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ab::refOffset, 1 }, "Reference Start",
        juce::NormalisableRange<float> { -600.0f, 3600.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ab::loudnessMatch, 1 }, "Loudness Match", true));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ab::peakMatch, 1 }, "Peak Match", false));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ab::monoCheck, 1 }, "Mono Check", false));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ab::loopRef, 1 }, "Loop Reference", true));

    return { parameters.begin(), parameters.end() };
}

//==============================================================================
ABmatchAudioProcessor::ABmatchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ABMATCH", ab::createLayout())
{
    cacheRawPointers();
}

void ABmatchAudioProcessor::cacheRawPointers()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    raw.monitor       = get (ab::monitor);
    raw.refTrim       = get (ab::refTrim);
    raw.refOffset     = get (ab::refOffset);
    raw.loudnessMatch = get (ab::loudnessMatch);
    raw.peakMatch     = get (ab::peakMatch);
    raw.monoCheck     = get (ab::monoCheck);
    raw.loopRef       = get (ab::loopRef);
}

void ABmatchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int blockSize = juce::jmax (samplesPerBlock, 512);

    currentSampleRate = sampleRate;
    mixPeakHold = 0.0f;

    reference.prepare (sampleRate, blockSize);

    mixMeter.prepare (sampleRate);
    refMeter.prepare (sampleRate);
    mixSpectrum.prepare (sampleRate);
    refSpectrum.prepare (sampleRate);

    refBuffer.setSize (2, blockSize);
    refBuffer.clear();

    refMix.reset (sampleRate, 0.025);
    refMix.setCurrentAndTargetValue (0.0f);

    gainSmoothed.reset (sampleRate, 0.4);
    gainSmoothed.setCurrentAndTargetValue (1.0f);
}

void ABmatchAudioProcessor::releaseResources()
{
    mixMeter.reset();
    refMeter.reset();
}

bool ABmatchAudioProcessor::isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

//==============================================================================
void ABmatchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    const bool monitorRef = isMonitoringReference() && reference.isLoaded();
    const bool monoOn = raw.monoCheck->load() > 0.5f;
    const bool loudnessOn = raw.loudnessMatch->load() > 0.5f;
    const bool peakOn = raw.peakMatch->load() > 0.5f;
    const bool loopOn = raw.loopRef->load() > 0.5f;
    const float trimDb = raw.refTrim->load();
    const double offsetSeconds = raw.refOffset != nullptr ? (double) raw.refOffset->load() : 0.0;

    // ---- measure the incoming mix -----------------------------------------
    mixMeter.process (buffer.getArrayOfReadPointers(), channels, numSamples);
    mixLufsShort.store (mixMeter.getShortTerm());
    mixLufsIntegrated.store (mixMeter.getIntegrated());

    mixSpectrum.push (buffer.getReadPointer (0),
                      channels >= 2 ? buffer.getReadPointer (1) : nullptr,
                      numSamples);

    float mixPeak = 0.0f;

    for (int ch = 0; ch < channels; ++ch)
        mixPeak = juce::jmax (mixPeak, buffer.getMagnitude (ch, 0, numSamples));

    // Slow peak hold (-3 dB every 3 s) so peak matching tracks the programme
    // material rather than jumping with each block's peak sample.
    mixPeakHold = juce::jmax (mixPeak, mixPeakHold * std::pow (0.5f, (float) numSamples
                                                              / (float) (currentSampleRate * 3.0)));

    // ---- reference level matching -----------------------------------------
    float targetDb = 0.0f;

    if (loudnessOn)
    {
        const float mixSt = mixMeter.getShortTerm();
        const float refLufs = reference.getIntegratedLufs();

        if (mixSt > -60.0f && refLufs > -60.0f)
            targetDb = mixSt - refLufs;
    }
    else if (peakOn)
    {
        const float refPeak = reference.getPeakDb();

        if (mixPeakHold > 1.0e-5f && refPeak > -60.0f)
            targetDb = juce::Decibels::gainToDecibels (mixPeakHold, -100.0f) - refPeak;
    }

    targetDb = juce::jlimit (-24.0f, 24.0f, targetDb + trimDb);

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (targetDb));
    appliedGainDb.store (targetDb);

    // ---- reference playback, locked to the host transport -----------------
    reference.setLooping (loopOn);

    juce::int64 hostSample = -1;
    bool transportPlaying = true;

    if (auto* playhead = getPlayHead())
    {
        if (auto info = playhead->getPosition())
        {
            transportPlaying = info->getIsPlaying();

            if (auto time = info->getTimeInSamples())
            {
                hostSample = *time;
                transportSeconds.store ((double) hostSample / currentSampleRate);
            }
        }
    }

    if (refBuffer.getNumSamples() != numSamples)
        refBuffer.setSize (2, numSamples, false, false, true);

    // The reference is rendered whenever the transport runs, not only while it
    // is monitored, so its spectrum and level stay live for the comparison view.
    // Its position is the transport time minus the chosen start point, so the
    // file can be lined up with any bar in the project.
    juce::int64 refStartSample = -1;   // -1 = free-running (no transport position)
    bool refAudible = transportPlaying;

    if (refAudible && hostSample >= 0)
    {
        const double refSeconds = (double) hostSample / currentSampleRate - offsetSeconds;

        if (refSeconds < 0.0)
            refAudible = false;        // the transport is still before the start point
        else
            refStartSample = (juce::int64) std::llround (refSeconds * currentSampleRate);
    }

    if (refAudible)
    {
        reference.render (refBuffer, numSamples, refStartSample);

        refMeter.process (refBuffer.getArrayOfReadPointers(), 2, numSamples);
        refLufsShort.store (refMeter.getShortTerm());
    }
    else
    {
        refBuffer.clear();
    }

    refSpectrum.push (refBuffer.getReadPointer (0), refBuffer.getReadPointer (1), numSamples);

    // ---- blend the monitored source in ------------------------------------
    refMix.setTargetValue (monitorRef ? 1.0f : 0.0f);

    if (refMix.getCurrentValue() > 0.0001f || refMix.getTargetValue() > 0.0001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = refMix.getNextValue();
            const float gain = gainSmoothed.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                auto* out = buffer.getWritePointer (ch);
                out[i] += (refBuffer.getSample (juce::jmin (ch, 1), i) * gain - out[i]) * mix;
            }
        }
    }
    else
    {
        refMix.skip (numSamples);
        gainSmoothed.skip (numSamples);
    }

    // ---- mono check -------------------------------------------------------
    if (monoOn && channels >= 2)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = 0.5f * (left[i] + right[i]);
            left[i] = mid;
            right[i] = mid;
        }
    }

    // ---- output metering --------------------------------------------------
    float outPeak = 0.0f;
    double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = buffer.getSample (0, i);
        const float r = channels >= 2 ? buffer.getSample (1, i) : l;

        outPeak = juce::jmax (outPeak, juce::jmax (std::abs (l), std::abs (r)));
        sumLR += (double) l * r;
        sumLL += (double) l * l;
        sumRR += (double) r * r;
    }

    outPeakDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f));

    if (sumLL > 1.0e-12 && sumRR > 1.0e-12)
    {
        const float instant = (float) (sumLR / std::sqrt (sumLL * sumRR));
        correlation.store (correlation.load() + (juce::jlimit (-1.0f, 1.0f, instant) - correlation.load()) * 0.2f);
    }
}

//==============================================================================
bool ABmatchAudioProcessor::isMonitoringReference() const
{
    return raw.monitor != nullptr && raw.monitor->load() > 0.5f;
}

void ABmatchAudioProcessor::getSpectrum (int source, float* dest) const
{
    if (source == 1)
        refSpectrum.copyBins (dest);
    else
        mixSpectrum.copyBins (dest);
}

ABmatchAudioProcessor::Meters ABmatchAudioProcessor::getMeters() const
{
    Meters m;
    m.peakDb = outPeakDb.load();
    m.correlation = correlation.load();

    if (isMonitoringReference())
    {
        m.lufsShort = refLufsShort.load();
        m.lufsIntegrated = reference.getIntegratedLufs();
    }
    else
    {
        m.lufsShort = mixLufsShort.load();
        m.lufsIntegrated = mixLufsIntegrated.load();
    }

    return m;
}

//==============================================================================
bool ABmatchAudioProcessor::loadReference (const juce::File& file, juce::String& error)
{
    if (! reference.load (file, getSampleRate() > 0.0 ? getSampleRate() : 48000.0, &error))
        return false;

    refMeter.reset();
    return true;
}

void ABmatchAudioProcessor::unloadReference()
{
    reference.unload();
    refMeter.reset();
}

//==============================================================================
void ABmatchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
    {
        const auto refFile = reference.getFile();

        if (refFile.existsAsFile())
            xml->setAttribute ("refPath", refFile.getFullPathName());

        copyXmlToBinary (*xml, destData);
    }
}

void ABmatchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto refPath = xml->getStringAttribute ("refPath");
    xml->removeAttribute ("refPath");

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (refPath.isNotEmpty())
    {
        const juce::File refFile (refPath);

        if (refFile.existsAsFile())
        {
            juce::String ignored;
            reference.load (refFile, getSampleRate() > 0.0 ? getSampleRate() : 48000.0, &ignored);
            refMeter.reset();
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ABmatchAudioProcessor::createEditor()
{
    return new ABmatchAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ABmatchAudioProcessor();
}
