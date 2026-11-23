#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ab
{

/** Single source of truth for ABmatch's automatable parameters. */
inline constexpr const char* monitor       = "monitor";       // 0 = A (mix), 1 = B (reference)
inline constexpr const char* refTrim       = "refTrim";       // dB
inline constexpr const char* refOffset     = "refOffset";     // project time where the reference starts
inline constexpr const char* loudnessMatch = "loudnessMatch";
inline constexpr const char* peakMatch     = "peakMatch";
inline constexpr const char* monoCheck     = "monoCheck";
inline constexpr const char* loopRef       = "loopRef";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace ab
