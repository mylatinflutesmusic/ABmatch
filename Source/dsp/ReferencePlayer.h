#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <cmath>
#include <memory>

#include "LoudnessMeter.h"

namespace ab
{

/**
    Holds one reference track in memory at the host sample rate and plays it back
    in step with the host transport.

    The file is read and (if needed) resampled on the message thread. The audio
    thread only ever reads it behind a try-lock, so a load can never block the
    render callback: a block that collides with a load outputs silence instead.
*/
class ReferencePlayer
{
public:
    void prepare (double newSampleRate, int maxBlockSize)
    {
        maxBlock = juce::jmax (32, maxBlockSize);

        if (loaded && ! juce::approximatelyEqual (newSampleRate, sampleRate))
            if (auto f = getFile(); f.existsAsFile())
                load (f, newSampleRate, nullptr);

        sampleRate = newSampleRate;
    }

    bool load (const juce::File& fileToLoad, double hostSampleRate, juce::String* error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (fileToLoad));

        if (reader == nullptr)
        {
            if (error != nullptr)
                *error = "ABmatch could not read that file.";

            return false;
        }

        const auto lengthIn = (int) juce::jmin ((juce::int64) 400000000, reader->lengthInSamples);

        if (lengthIn <= 0)
        {
            if (error != nullptr)
                *error = "That file is empty.";

            return false;
        }

        juce::AudioBuffer<float> source (2, lengthIn);
        source.clear();
        reader->read (&source, 0, lengthIn, 0, true, reader->numChannels > 1);

        if (reader->numChannels <= 1)
            source.copyFrom (1, 0, source, 0, 0, lengthIn);

        const double fileRate = reader->sampleRate > 0.0 ? reader->sampleRate : hostSampleRate;

        std::unique_ptr<juce::AudioBuffer<float>> converted (new juce::AudioBuffer<float> (2, lengthIn));

        if (! juce::approximatelyEqual (fileRate, hostSampleRate))
        {
            const int outLength = juce::jmax (1, (int) std::llround ((double) lengthIn
                                                                     * hostSampleRate / fileRate));

            converted->setSize (2, outLength);
            converted->clear();

            const double ratio = fileRate / hostSampleRate;

            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.reset();
                interpolator.process (ratio, source.getReadPointer (ch),
                                      converted->getWritePointer (ch), outLength);
            }
        }
        else
        {
            *converted = std::move (source);
        }

        // Measure the whole file once, for loudness / peak matching and the UI.
        float measuredLufs = -100.0f, measuredPeak = -100.0f;

        {
            LoudnessMeter meter;
            meter.prepare (hostSampleRate);

            const int total = converted->getNumSamples();
            const int chunk = 1024;

            for (int pos = 0; pos < total; pos += chunk)
            {
                const int n = juce::jmin (chunk, total - pos);
                const float* channels[2] = { converted->getReadPointer (0, pos),
                                             converted->getReadPointer (1, pos) };
                meter.process (channels, 2, n);
            }

            measuredLufs = meter.getIntegrated();
            measuredPeak = juce::Decibels::gainToDecibels (
                juce::jmax (converted->getMagnitude (0, 0, total),
                            converted->getMagnitude (1, 0, total)), -100.0f);
        }

        {
            const juce::SpinLock::ScopedLockType guard (lock);

            data = std::move (*converted);
            file = fileToLoad;
            name = fileToLoad.getFileName();
            position = 0;
            loaded = true;
            sampleRate = hostSampleRate;
        }

        integratedLufs.store (measuredLufs);
        peakDb.store (measuredPeak);

        return true;
    }

    void unload()
    {
        const juce::SpinLock::ScopedLockType guard (lock);

        data.setSize (0, 0);
        file = juce::File();
        name = {};
        position = 0;
        loaded = false;

        integratedLufs.store (-100.0f);
        peakDb.store (-100.0f);
    }

    bool isLoaded() const noexcept { return loaded.load(); }

    juce::File getFile() const
    {
        const juce::SpinLock::ScopedLockType guard (lock);
        return file;
    }

    juce::String getName() const
    {
        const juce::SpinLock::ScopedLockType guard (lock);
        return name;
    }

    double getLengthSeconds() const
    {
        const juce::SpinLock::ScopedLockType guard (lock);
        return sampleRate > 0.0 ? (double) data.getNumSamples() / sampleRate : 0.0;
    }

    double getPositionSeconds() const
    {
        const juce::SpinLock::ScopedTryLockType guard (lock);
        return guard.isLocked() && sampleRate > 0.0 ? (double) position / sampleRate : 0.0;
    }

    float getIntegratedLufs() const noexcept { return integratedLufs.load(); }
    float getPeakDb() const noexcept         { return peakDb.load(); }

    void setLooping (bool shouldLoop) noexcept { looping = shouldLoop; }

    /** Fills dest with the reference. startSample < 0 keeps free-running. */
    void render (juce::AudioBuffer<float>& dest, int numSamples, juce::int64 startSample)
    {
        dest.clear();

        if (numSamples <= 0)
            return;

        const juce::SpinLock::ScopedTryLockType guard (lock);

        if (! guard.isLocked() || ! loaded || data.getNumSamples() <= 0)
            return;

        const int total = data.getNumSamples();
        const int channels = juce::jmin (2, dest.getNumChannels());

        if (startSample >= 0)
        {
            if (! looping)
            {
                // Past the end with looping off: stay silent rather than wrapping.
                if (startSample >= total)
                {
                    position = total;
                    return;
                }

                position = (int) startSample;
            }
            else
            {
                position = (int) (startSample % total);
            }
        }

        if (position >= total)
        {
            if (! looping)
                return;

            position = 0;
        }

        int pos = position;

        for (int i = 0; i < numSamples; ++i)
        {
            if (pos >= total)
            {
                if (! looping)
                    break;

                pos = 0;
            }

            for (int ch = 0; ch < channels; ++ch)
                dest.getWritePointer (ch)[i] = data.getReadPointer (ch)[pos];

            ++pos;
        }

        position = juce::jmin (pos, total);
    }

private:
    mutable juce::SpinLock lock;

    juce::AudioBuffer<float> data;
    juce::File file;
    juce::String name;

    double sampleRate = 48000.0;
    int position = 0;
    int maxBlock = 512;
    bool looping = true;

    std::atomic<bool> loaded { false };
    std::atomic<float> integratedLufs { -100.0f };
    std::atomic<float> peakDb { -100.0f };
};

} // namespace ab
