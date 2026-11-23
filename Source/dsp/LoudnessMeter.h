#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace ab
{

/**
    BS.1770-4 style loudness measurement (K-weighting, 400 ms gated blocks,
    integrated + 3 s short-term). Publishes results as atomics so the editor can
    read them from another thread.
*/
class LoudnessMeter
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };

        auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 1681.974450955533, 0.7071752369554196,
            juce::Decibels::decibelsToGain (3.999843853973347f));

        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sampleRate, 38.13547087602444, 0.5003270373238773);

        for (int ch = 0; ch < 2; ++ch)
        {
            shelf[ch].prepare (spec);
            highPass[ch].prepare (spec);
            shelf[ch].coefficients = shelfCoeffs;
            highPass[ch].coefficients = hpCoeffs;
        }

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            shelf[ch].reset();
            highPass[ch].reset();
        }

        slots.fill (0.0f);
        slotIndex = 0;
        slotFill = 0;
        slotSum = 0.0f;
        samplesInSlot = 0;
        blocks.clear();
        integratedLufs.store (-100.0f);
        shortTermLufs.store (-100.0f);
        updatesSinceIntegrated = 0;
    }

    void process (const float* const* channels, int numChannels, int numSamples)
    {
        const int channelsToUse = juce::jmin (2, numChannels);
        const int slotLength = juce::jmax (1, (int) std::lround (sampleRate * 0.1));

        for (int i = 0; i < numSamples; ++i)
        {
            float energy = 0.0f;

            for (int ch = 0; ch < channelsToUse; ++ch)
            {
                float s = channels[ch][i];
                s = shelf[ch].processSample (s);
                s = highPass[ch].processSample (s);
                energy += s * s;
            }

            slotSum += energy;

            if (++samplesInSlot >= slotLength)
                closeSlot();
        }
    }

    float getIntegrated() const noexcept  { return integratedLufs.load(); }
    float getShortTerm() const noexcept   { return shortTermLufs.load(); }

private:
    void closeSlot()
    {
        const float energy = slotSum / (float) juce::jmax (1, samplesInSlot);

        slots[(size_t) slotIndex] = energy;
        slotIndex = (slotIndex + 1) % (int) slots.size();
        slotFill = juce::jmin ((int) slots.size(), slotFill + 1);

        slotSum = 0.0f;
        samplesInSlot = 0;

        if (slotFill >= 4)
        {
            const float blockEnergy = averageSlots (4);
            const float lufs = energyToLufs (blockEnergy);

            blocks.push_back (lufs);

            if (blocks.size() > 36000)
                blocks.erase (blocks.begin(), blocks.begin() + 1200);

            const float stEnergy = averageSlots (juce::jmin (30, slotFill));
            shortTermLufs.store (energyToLufs (stEnergy));

            if (++updatesSinceIntegrated >= 5)
            {
                updatesSinceIntegrated = 0;
                integratedLufs.store (computeIntegrated());
            }
        }
    }

    float averageSlots (int count) const
    {
        if (count <= 0)
            return 0.0f;

        float sum = 0.0f;
        const int size = (int) slots.size();

        for (int k = 1; k <= count; ++k)
            sum += slots[(size_t) ((slotIndex - k + size * 2) % size)];

        return sum / (float) count;
    }

    static float energyToLufs (float energy)
    {
        return energy > 1.0e-12f ? -0.691f + 10.0f * std::log10 (energy) : -100.0f;
    }

    float computeIntegrated() const
    {
        constexpr float absoluteGate = -70.0f;

        double sum = 0.0;
        int count = 0;

        for (float l : blocks)
        {
            if (l > absoluteGate)
            {
                sum += std::pow (10.0, (double) l / 10.0);
                ++count;
            }
        }

        if (count == 0)
            return -100.0f;

        const float relativeGate = (float) (10.0 * std::log10 (sum / count)) - 10.0f;

        double gatedSum = 0.0;
        int gatedCount = 0;

        for (float l : blocks)
        {
            if (l > absoluteGate && l > relativeGate)
            {
                gatedSum += std::pow (10.0, (double) l / 10.0);
                ++gatedCount;
            }
        }

        if (gatedCount == 0)
            return -100.0f;

        return (float) (10.0 * std::log10 (gatedSum / gatedCount));
    }

    double sampleRate = 44100.0;

    std::array<juce::dsp::IIR::Filter<float>, 2> shelf;
    std::array<juce::dsp::IIR::Filter<float>, 2> highPass;

    std::array<float, 40> slots {}; // 4 seconds of 100 ms slots
    int slotIndex = 0;
    int slotFill = 0;
    float slotSum = 0.0f;
    int samplesInSlot = 0;

    std::vector<float> blocks;
    int updatesSinceIntegrated = 0;

    std::atomic<float> integratedLufs { -100.0f };
    std::atomic<float> shortTermLufs { -100.0f };
};

} // namespace ab
