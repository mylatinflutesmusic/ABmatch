#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace ab
{

/**
    Short FFT spectrum with log-spaced display bins.

    Frames are smoothed over time with a fast attack and a slow release, and
    narrow dips between neighbouring bands are filled so the curve reads as a
    shape instead of a comb. The band smoothing takes the loudest neighbour
    rather than an average: averaging in dB crushes narrow peaks (a full-scale
    sine measured 48 dB low), while a maximum keeps the level honest.

    Results are published as atomics so the editor can read them at timer rate
    without touching the audio thread's state.
*/
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 2;
    static constexpr int numBins = 160;

    static constexpr float attackCoefficient = 0.7f;   // fast rise
    static constexpr float releaseCoefficient = 0.15f; // slow, smooth fall
    static constexpr float lowestFrequency = 20.0f;
    static constexpr float highestFrequency = 20000.0f;

    SpectrumAnalyzer()
    {
        for (auto& b : bins)
            b.store (-100.0f);
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        fifo.assign ((size_t) fftSize, 0.0f);
        fftData.assign ((size_t) fftSize * 2, 0.0f);

        writePos = 0;

        window = std::make_unique<juce::dsp::WindowingFunction<float>> (
            fftSize, juce::dsp::WindowingFunction<float>::hann, false);

        smoothed.fill (-100.0f);

        for (auto& b : bins)
            b.store (-100.0f);
    }

    void reset()
    {
        std::fill (fifo.begin(), fifo.end(), 0.0f);
        writePos = 0;

        smoothed.fill (-100.0f);

        for (auto& b : bins)
            b.store (-100.0f);
    }

    void push (const float* left, const float* right, int numSamples)
    {
        if (fifo.empty() || left == nullptr || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            fifo[(size_t) writePos] = 0.5f * (left[i] + (right != nullptr ? right[i] : left[i]));

            if (++writePos >= fftSize)
            {
                analyse();

                std::memmove (fifo.data(), fifo.data() + hopSize,
                              sizeof (float) * (size_t) hopSize);

                writePos = hopSize;
            }
        }
    }

    void copyBins (float* dest) const
    {
        if (dest == nullptr)
            return;

        for (int i = 0; i < numBins; ++i)
            dest[i] = bins[(size_t) i].load();
    }

    static float binFrequency (float index)
    {
        const float t = index / (float) (numBins - 1);
        return lowestFrequency * std::pow (highestFrequency / lowestFrequency, t);
    }

private:
    void analyse()
    {
        std::copy (fifo.begin(), fifo.end(), fftData.begin());
        std::fill (fftData.begin() + fftSize, fftData.begin() + fftSize * 2, 0.0f);

        window->multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const int maxBin = fftSize / 2 - 1;

        for (int b = 0; b < numBins; ++b)
        {
            const float f0 = binFrequency ((float) b);
            const float f1 = binFrequency ((float) (b + 1));

            int i0 = (int) std::floor (f0 * (float) fftSize / (float) sampleRate);
            int i1 = (int) std::ceil (f1 * (float) fftSize / (float) sampleRate);

            i0 = juce::jlimit (0, maxBin, i0);
            i1 = juce::jlimit (i0 + 1, maxBin + 1, i1);

            float peak = 0.0f;

            for (int i = i0; i < i1; ++i)
                peak = juce::jmax (peak, fftData[(size_t) i]);

            // Hann window has a coherent gain of 0.5, and a sine of amplitude A
            // peaks at A * fftSize / 4, so 4/N maps a full-scale sine to 0 dB.
            frame[(size_t) b] = juce::Decibels::gainToDecibels (peak * (4.0f / (float) fftSize), -100.0f);
        }

        // Fill narrow dips: each band first takes the loudest of its two
        // neighbours, so single-bin notches disappear but real peaks keep their
        // level. The following 1-2-1 average then rounds the plateau's shoulders
        // without touching its top, because the plateau is flat.
        std::array<float, numBins> temp {};

        for (int b = 0; b < numBins; ++b)
        {
            const float loudest = juce::jmax (frame[(size_t) b],
                                              frame[(size_t) juce::jlimit (0, numBins - 1, b - 1)],
                                              frame[(size_t) juce::jlimit (0, numBins - 1, b + 1)]);
            temp[(size_t) b] = loudest;
        }

        for (int b = 0; b < numBins; ++b)
        {
            const float left = temp[(size_t) juce::jlimit (0, numBins - 1, b - 1)];
            const float right = temp[(size_t) juce::jlimit (0, numBins - 1, b + 1)];
            frame[(size_t) b] = 0.25f * left + 0.5f * temp[(size_t) b] + 0.25f * right;
        }

        // Temporal smoothing: jumps up quickly, glides down slowly.
        for (int b = 0; b < numBins; ++b)
        {
            const float target = frame[(size_t) b];
            float& value = smoothed[(size_t) b];
            const float coefficient = target > value ? attackCoefficient : releaseCoefficient;

            value += (target - value) * coefficient;
            bins[(size_t) b].store (value);
        }
    }


    juce::dsp::FFT fft { fftOrder };
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::vector<float> fifo, fftData;
    int writePos = 0;
    double sampleRate = 48000.0;

    std::array<float, numBins> frame {}, smoothed {};
    std::array<std::atomic<float>, numBins> bins;
};

} // namespace ab
