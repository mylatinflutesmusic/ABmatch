#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"

namespace abui
{

class ABLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ABLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

//==============================================================================
class Panel : public juce::Component
{
public:
    Panel (const juce::String& titleText, juce::Colour accentColour);

    void paint (juce::Graphics&) override;

    juce::String title;
    juce::Colour accent;
};

//==============================================================================
class MeterPanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;

    void update (const ABmatchAudioProcessor::Meters& newMeters, bool monitoringRef,
                 float referenceLufs, float referencePeakDb);

private:
    void drawBar (juce::Graphics&, juce::Rectangle<int> area, const juce::String& label,
                  float value, float minValue, float maxValue, juce::Colour colour,
                  const juce::String& readout, bool centreZero);

    ABmatchAudioProcessor::Meters meters;
    bool monitoringRef = false;
    float refLufs = -100.0f;
    float refPeakDb = -100.0f;
};

//==============================================================================
/** Overlaid A (mix) and B (reference) spectra on a log frequency axis. */
class SpectrumPanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;

    void update (const float* mixBins, const float* refBins,
                 bool showMix, bool showRef, bool referenceAvailable);

private:
    void drawCurve (juce::Graphics&, juce::Colour colour, const float* values) const;

    std::array<float, ABmatchAudioProcessor::spectrumBins> mixValues {};
    std::array<float, ABmatchAudioProcessor::spectrumBins> refValues {};

    bool showMixCurve = true;
    bool showRefCurve = true;
    bool refAvailable = false;
};

} // namespace abui

//==============================================================================
class ABmatchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ABmatchAudioProcessorEditor (ABmatchAudioProcessor&);
    ~ABmatchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMonitor (int source);
    void updateFileLabels();
    void chooseReference();

    ABmatchAudioProcessor& processor;
    abui::ABLookAndFeel lookAndFeel;

    juce::Component content;
    juce::AffineTransform contentTransform;

    abui::Panel referencePanel { "REFERENCE", juce::Colour (0xffffb454) };
    abui::Panel monitorPanel   { "MONITOR",   juce::Colour (0xff5ec8f2) };
    abui::MeterPanel meterPanel;
    abui::SpectrumPanel spectrumPanel;

    juce::TextButton showAButton { "A" };
    juce::TextButton showBButton { "B" };

    std::array<float, ABmatchAudioProcessor::spectrumBins> mixSpectrumValues {};
    std::array<float, ABmatchAudioProcessor::spectrumBins> refSpectrumValues {};

    juce::TextButton mixButton { "A   MIX" };
    juce::TextButton refButton { "B   REF" };

    juce::Label fileNameLabel;
    juce::Label fileInfoLabel;

    juce::TextButton loadButton { "LOAD REFERENCE" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton loopButton { "LOOP" };

    juce::Label offsetLabel;
    juce::Slider offsetSlider;
    juce::TextButton playheadButton { "FROM PLAYHEAD" };

    juce::TextButton loudnessButton { "LOUDNESS MATCH" };
    juce::TextButton peakButton { "PEAK MATCH" };
    juce::TextButton monoButton { "MONO CHECK" };

    juce::Label trimLabel;
    juce::Slider trimSlider;
    juce::Label matchLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loudnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> peakAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> offsetAttachment;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ABmatchAudioProcessorEditor)
};
