#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "Parameters.h"
#include "dsp/ReferencePlayer.h"
#include "dsp/SpectrumAnalyzer.h"

class ABmatchAudioProcessor : public juce::AudioProcessor
{
public:
    ABmatchAudioProcessor();
    ~ABmatchAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    bool loadReference (const juce::File& file, juce::String& error);
    void unloadReference();

    bool isReferenceLoaded() const          { return reference.isLoaded(); }
    juce::String getReferenceName() const   { return reference.getName(); }
    double getReferenceLengthSeconds() const { return reference.getLengthSeconds(); }
    float getReferenceLufs() const          { return reference.getIntegratedLufs(); }
    float getReferencePeakDb() const        { return reference.getPeakDb(); }

    bool isMonitoringReference() const;
    float getAppliedGainDb() const          { return appliedGainDb.load(); }

    /** Last transport position seen, in seconds (for "start at playhead"). */
    double getTransportSeconds() const      { return transportSeconds.load(); }

    struct Meters
    {
        float peakDb = -100.0f;
        float lufsShort = -100.0f;
        float lufsIntegrated = -100.0f;
        float correlation = 1.0f;
    };

    /** Meters for whichever source is currently being monitored. */
    Meters getMeters() const;

    static constexpr int spectrumBins = ab::SpectrumAnalyzer::numBins;

    /** Copies the smoothed spectrum of source 0 (mix) or 1 (reference) in dB. */
    void getSpectrum (int source, float* dest) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Raw
    {
        std::atomic<float>* monitor = nullptr;
        std::atomic<float>* refTrim = nullptr;
        std::atomic<float>* refOffset = nullptr;
        std::atomic<float>* loudnessMatch = nullptr;
        std::atomic<float>* peakMatch = nullptr;
        std::atomic<float>* monoCheck = nullptr;
        std::atomic<float>* loopRef = nullptr;
    };

    Raw raw;
    void cacheRawPointers();

    ab::ReferencePlayer reference;
    ab::LoudnessMeter mixMeter, refMeter;
    ab::SpectrumAnalyzer mixSpectrum, refSpectrum;
    juce::AudioBuffer<float> refBuffer;
    juce::SmoothedValue<float> refMix, gainSmoothed;

    double currentSampleRate = 48000.0;
    float mixPeakHold = 0.0f;

    std::atomic<float> appliedGainDb { 0.0f };
    std::atomic<double> transportSeconds { 0.0 };
    std::atomic<float> mixLufsShort { -100.0f };
    std::atomic<float> mixLufsIntegrated { -100.0f };
    std::atomic<float> refLufsShort { -100.0f };
    std::atomic<float> outPeakDb { -100.0f };
    std::atomic<float> correlation { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ABmatchAudioProcessor)
};
