#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    const juce::Colour bgTop        { 0xff10161d };
    const juce::Colour bgBottom     { 0xff070a0e };
    const juce::Colour panelBg      { 0xff151b22 };
    const juce::Colour panelBorder  { 0xff232c36 };
    const juce::Colour textBright   { 0xffe9eff5 };
    const juce::Colour textDim      { 0xff7a8794 };
    const juce::Colour trackBg      { 0xff242d37 };
    const juce::Colour fieldBg      { 0xff1b222b };
    const juce::Colour fieldBorder  { 0xff2c3641 };

    const juce::Colour mixColour    { 0xff5ec8f2 };
    const juce::Colour refColour    { 0xffffb454 };

    constexpr int editorWidth = 1320;
    constexpr int editorHeight = 640;

    constexpr float spectrumFloorDb = -84.0f;
    constexpr float minUIScale = 0.85f;

    juce::Font makeFont (float height)
    {
        return juce::Font (juce::FontOptions (height));
    }

    juce::String dbText (float value, const juce::String& suffix, int decimals = 1)
    {
        return value > -99.0f ? juce::String (value, decimals) + suffix : juce::String ("--") + suffix;
    }
}

namespace abui
{

//==============================================================================
ABLookAndFeel::ABLookAndFeel()
{
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc7d3df));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, refColour);
    setColour (juce::TextButton::buttonColourId, fieldBg);
    setColour (juce::TextButton::buttonOnColourId, mixColour);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff0b0f13));
}

void ABLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float rotaryStartAngle,
                                      float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.46f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float thickness = juce::jmax (2.0f, radius * 0.1f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (trackBg);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    const float bodyRadius = radius * 0.66f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                          .withCentre (centre);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b343f), body.getX(), body.getY(),
                                             juce::Colour (0xff141a21), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff39434f));
    g.drawEllipse (body, 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (centre.x, centre.y - bodyRadius * 0.2f);
    pointer.lineTo (centre.x, centre.y - bodyRadius * 0.95f);
    g.setColour (accent.interpolatedWith (juce::Colours::white, 0.3f));
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (1.6f, radius * 0.08f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void ABLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                          const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const auto onColour = button.findColour (juce::TextButton::buttonOnColourId);

    juce::Colour fill = on ? onColour : fieldBg;

    if (down)
        fill = fill.darker (0.2f);
    else if (highlighted)
        fill = fill.brighter (0.12f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? onColour.brighter (0.25f) : fieldBorder);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void ABLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? button.findColour (juce::TextButton::textColourOnId)
                                         : button.findColour (juce::TextButton::textColourOffId));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

juce::Font ABLookAndFeel::getLabelFont (juce::Label& label)
{
    return makeFont (juce::jlimit (9.5f, 17.0f, (float) label.getHeight() * 0.62f));
}

juce::Font ABLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return makeFont (juce::jlimit (9.0f, 13.0f, (float) buttonHeight * 0.38f));
}

//==============================================================================
Panel::Panel (const juce::String& titleText, juce::Colour accentColour)
    : title (titleText), accent (accentColour)
{
    setInterceptsMouseClicks (false, true);
}

void Panel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (panelBg);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (12.0f, 11.0f, 3.0f, 12.0f, 1.5f);

    g.setColour (textBright);
    g.setFont (makeFont (12.0f));
    g.drawText (title, 22, 8, getWidth() - 40, 20, juce::Justification::centredLeft, false);
}

//==============================================================================
void MeterPanel::update (const ABmatchAudioProcessor::Meters& newMeters, bool monitoringReference,
                         float referenceLufs, float referencePeakDb)
{
    meters = newMeters;
    monitoringRef = monitoringReference;
    refLufs = referenceLufs;
    refPeakDb = referencePeakDb;
}

void MeterPanel::drawBar (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                          float value, float minValue, float maxValue, juce::Colour colour,
                          const juce::String& readout, bool centreZero)
{
    auto textRow = area.removeFromTop (18);

    g.setColour (textDim);
    g.setFont (makeFont (9.5f));
    g.drawText (label, textRow, juce::Justification::centredLeft);

    g.setColour (textBright);
    g.setFont (makeFont (11.5f));
    g.drawText (readout, textRow, juce::Justification::centredRight);

    auto bar = area.removeFromTop (9).toFloat();

    g.setColour (trackBg);
    g.fillRoundedRectangle (bar, 4.5f);

    const float t = juce::jlimit (0.0f, 1.0f,
                                  (value - minValue) / juce::jmax (0.0001f, maxValue - minValue));

    if (centreZero)
    {
        const float centre = bar.getCentreX();
        const float halfWidth = bar.getWidth() * 0.5f;
        const float end = centre + (t - 0.5f) * 2.0f * halfWidth;
        juce::Rectangle<float> fill (juce::jmin (centre, end), bar.getY(),
                                     std::abs (end - centre), bar.getHeight());
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 4.5f);
        g.setColour (fieldBorder);
        g.drawVerticalLine ((int) centre, bar.getY(), bar.getBottom());
    }
    else
    {
        g.setColour (colour);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 4.5f);
    }
}

void MeterPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (panelBg);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff9aa7b4));
    g.fillRoundedRectangle (12.0f, 11.0f, 3.0f, 12.0f, 1.5f);

    g.setColour (textBright);
    g.setFont (makeFont (12.0f));
    g.drawText ("METERS", 22, 8, getWidth() - 160, 20, juce::Justification::centredLeft, false);

    // Which source the meters refer to.
    const auto badge = juce::Rectangle<int> (getWidth() - 116, 8, 96, 20);
    g.setColour (monitoringRef ? refColour : mixColour);
    g.fillRoundedRectangle (badge.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff0b0f13));
    g.setFont (makeFont (9.5f));
    g.drawText (monitoringRef ? "B  REFERENCE" : "A  MIX", badge,
                juce::Justification::centred, false);

    auto area = getLocalBounds().reduced (18, 0);
    area.removeFromTop (46);

    drawBar (g, area.removeFromTop (84), "PEAK", meters.peakDb, -60.0f, 0.0f,
             meters.peakDb > -1.0f ? juce::Colour (0xffff7d7d) : mixColour,
             dbText (meters.peakDb, " dBFS"), false);

    // ---- Loudness ---------------------------------------------------------
    {
        auto lufs = area.removeFromTop (152);

        g.setColour (textDim);
        g.setFont (makeFont (9.5f));
        g.drawText ("LOUDNESS  SHORT-TERM", lufs.removeFromTop (18),
                    juce::Justification::centredLeft);

        g.setColour (textBright);
        g.setFont (makeFont (34.0f));
        g.drawText (meters.lufsShort > -99.0f ? juce::String (meters.lufsShort, 1) : juce::String ("--"),
                    lufs.removeFromTop (44), juce::Justification::centredLeft);

        g.setColour (textDim);
        g.setFont (makeFont (10.0f));
        g.drawText ("LUFS", lufs.removeFromTop (16), juce::Justification::centredLeft);

        g.setFont (makeFont (10.5f));
        g.drawText ("INTEGRATED  " + dbText (meters.lufsIntegrated, " LUFS"),
                    lufs.removeFromTop (18), juce::Justification::centredLeft);
    }

    drawBar (g, area.removeFromTop (84), "CORRELATION", meters.correlation, -1.0f, 1.0f,
             juce::Colour (0xffb291f5),
             meters.correlation >= 0.0f ? "+" + juce::String (meters.correlation, 2)
                                        : juce::String (meters.correlation, 2), true);

    auto info = area.removeFromTop (96);
    g.setColour (textDim);
    g.setFont (makeFont (9.5f));
    g.drawText ("REFERENCE FILE", info.removeFromTop (16), juce::Justification::centredLeft);

    g.setColour (textBright);
    g.setFont (makeFont (12.0f));
    g.drawText (refLufs > -99.0f ? juce::String (refLufs, 1) + " LUFS" : juce::String ("--"),
                info.removeFromTop (20), juce::Justification::centredLeft);
    g.setFont (makeFont (10.5f));
    g.drawText (refPeakDb > -99.0f ? juce::String (refPeakDb, 1) + " dBFS peak" : juce::String ("no file"),
                info.removeFromTop (18), juce::Justification::centredLeft);
}

//==============================================================================
void SpectrumPanel::update (const float* mixBins, const float* refBins,
                            bool showMix, bool showRef, bool referenceAvailable)
{
    if (mixBins != nullptr)
        std::copy (mixBins, mixBins + ABmatchAudioProcessor::spectrumBins, mixValues.begin());

    if (refBins != nullptr)
        std::copy (refBins, refBins + ABmatchAudioProcessor::spectrumBins, refValues.begin());

    showMixCurve = showMix;
    showRefCurve = showRef;
    refAvailable = referenceAvailable;
}

void SpectrumPanel::drawCurve (juce::Graphics& g, juce::Colour colour, const float* values) const
{
    auto area = getLocalBounds().reduced (18, 0);
    area.removeFromTop (44);
    area.removeFromBottom (22);

    const auto plot = area.toFloat();
    juce::Path path;

    for (int i = 0; i < ABmatchAudioProcessor::spectrumBins; ++i)
    {
        const float x = plot.getX() + (float) i / (float) (ABmatchAudioProcessor::spectrumBins - 1)
                                         * plot.getWidth();
        const float value = juce::jlimit (spectrumFloorDb, 0.0f, values[i]);
        const float y = plot.getBottom() - (value - spectrumFloorDb) / -spectrumFloorDb * plot.getHeight();

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    juce::Path filled (path);
    filled.lineTo (plot.getRight(), plot.getBottom());
    filled.lineTo (plot.getX(), plot.getBottom());
    filled.closeSubPath();

    g.setColour (colour.withAlpha (0.13f));
    g.fillPath (filled);

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void SpectrumPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (panelBg);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xffb291f5));
    g.fillRoundedRectangle (12.0f, 11.0f, 3.0f, 12.0f, 1.5f);

    g.setColour (textBright);
    g.setFont (makeFont (12.0f));
    g.drawText ("SPECTRUM   A / B", 22, 8, 210, 20, juce::Justification::centredLeft, false);

    auto area = getLocalBounds().reduced (18, 0);
    area.removeFromTop (44);
    area.removeFromBottom (22);

    const auto plot = area.toFloat();

    // ---- dB grid ---------------------------------------------------------
    g.setFont (makeFont (8.5f));

    for (int db = 0; db >= (int) spectrumFloorDb; db -= 12)
    {
        const float y = plot.getBottom() - ((float) db - spectrumFloorDb) / -spectrumFloorDb * plot.getHeight();

        g.setColour (juce::Colour (0xff1e2832));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());

        g.setColour (textDim);
        g.drawText (juce::String (db), (int) plot.getX() + 4, (int) y - 11, 32, 11,
                    juce::Justification::centredLeft, false);
    }

    // ---- frequency grid --------------------------------------------------
    const float frequencies[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const float rangeRatio = std::log (ab::SpectrumAnalyzer::highestFrequency
                                       / ab::SpectrumAnalyzer::lowestFrequency);

    for (float f : frequencies)
    {
        const float t = std::log (f / ab::SpectrumAnalyzer::lowestFrequency) / rangeRatio;
        const float x = plot.getX() + t * plot.getWidth();

        g.setColour (juce::Colour (0xff1e2832));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

        g.setColour (textDim);
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                    (int) x - 15, (int) plot.getBottom() + 3, 30, 11,
                    juce::Justification::centred, false);
    }

    // ---- curves ----------------------------------------------------------
    if (showRefCurve && refAvailable)
        drawCurve (g, refColour, refValues.data());

    if (showMixCurve)
        drawCurve (g, mixColour, mixValues.data());

    if (showRefCurve && ! refAvailable)
    {
        g.setColour (textDim);
        g.setFont (makeFont (10.0f));
        g.drawText ("Load a reference to compare spectra", plot.toNearestInt(),
                    juce::Justification::centred, false);
    }
}

} // namespace abui

//==============================================================================
ABmatchAudioProcessorEditor::ABmatchAudioProcessorEditor (ABmatchAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);

    content.addAndMakeVisible (referencePanel);
    content.addAndMakeVisible (monitorPanel);
    content.addAndMakeVisible (meterPanel);
    content.addAndMakeVisible (spectrumPanel);

    for (auto* button : { &showAButton, &showBButton })
    {
        spectrumPanel.addAndMakeVisible (*button);
        button->setClickingTogglesState (true);
        button->setToggleState (true, juce::dontSendNotification);
    }

    showAButton.setColour (juce::TextButton::buttonOnColourId, mixColour);
    showBButton.setColour (juce::TextButton::buttonOnColourId, refColour);

    auto setUpSourceButton = [this] (juce::TextButton& button, juce::Colour accent, int source)
    {
        content.addAndMakeVisible (button);
        button.setClickingTogglesState (false);
        button.setColour (juce::TextButton::buttonOnColourId, accent);
        button.onClick = [this, source] { setMonitor (source); };
    };

    setUpSourceButton (mixButton, mixColour, 0);
    setUpSourceButton (refButton, refColour, 1);

    // ---- Reference panel -------------------------------------------------
    referencePanel.addAndMakeVisible (fileNameLabel);
    fileNameLabel.setText ("No reference loaded", juce::dontSendNotification);
    fileNameLabel.setFont (makeFont (16.0f));
    fileNameLabel.setColour (juce::Label::textColourId, textBright);
    fileNameLabel.setJustificationType (juce::Justification::centredLeft);
    fileNameLabel.setMinimumHorizontalScale (0.6f);

    referencePanel.addAndMakeVisible (fileInfoLabel);
    fileInfoLabel.setFont (makeFont (10.5f));
    fileInfoLabel.setColour (juce::Label::textColourId, textDim);
    fileInfoLabel.setJustificationType (juce::Justification::centredLeft);

    auto setUpToggle = [this] (juce::TextButton& button, abui::Panel& panel,
                               const char* parameterId, juce::Colour accent)
    {
        panel.addAndMakeVisible (button);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, accent);
        return std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, parameterId, button);
    };

    loopAttachment = setUpToggle (loopButton, referencePanel, ab::loopRef, refColour);

    referencePanel.addAndMakeVisible (loadButton);
    loadButton.setColour (juce::TextButton::buttonOnColourId, refColour);
    loadButton.onClick = [this] { chooseReference(); };

    referencePanel.addAndMakeVisible (clearButton);
    clearButton.onClick = [this] { processor.unloadReference(); };

    referencePanel.addAndMakeVisible (offsetLabel);
    offsetLabel.setText ("START AT", juce::dontSendNotification);
    offsetLabel.setFont (makeFont (9.5f));
    offsetLabel.setColour (juce::Label::textColourId, textDim);
    offsetLabel.setJustificationType (juce::Justification::centredLeft);

    referencePanel.addAndMakeVisible (offsetSlider);
    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 84, 22);
    offsetSlider.setTextValueSuffix (" s");
    offsetSlider.setDoubleClickReturnValue (true, 0.0);
    offsetSlider.setTooltip ("Project time where the reference starts. Negative values skip into the file.");
    offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ab::refOffset, offsetSlider);

    referencePanel.addAndMakeVisible (playheadButton);
    playheadButton.onClick = [this]
    {
        if (auto* parameter = processor.apvts.getParameter (ab::refOffset))
        {
            const auto range = parameter->getNormalisableRange();
            const auto seconds = juce::jlimit (range.start, range.end,
                                               (float) processor.getTransportSeconds());

            parameter->setValueNotifyingHost (range.convertTo0to1 (seconds));
        }
    };

    // ---- Monitor panel ---------------------------------------------------
    loudnessAttachment = setUpToggle (loudnessButton, monitorPanel, ab::loudnessMatch, mixColour);
    peakAttachment     = setUpToggle (peakButton, monitorPanel, ab::peakMatch, mixColour);
    monoAttachment     = setUpToggle (monoButton, monitorPanel, ab::monoCheck, juce::Colour (0xffb291f5));

    monitorPanel.addAndMakeVisible (trimLabel);
    trimLabel.setText ("REF TRIM", juce::dontSendNotification);
    trimLabel.setFont (makeFont (9.5f));
    trimLabel.setColour (juce::Label::textColourId, textDim);
    trimLabel.setJustificationType (juce::Justification::centred);

    monitorPanel.addAndMakeVisible (trimSlider);
    trimSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trimSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 14);
    trimSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
    trimSlider.setColour (juce::Slider::rotarySliderFillColourId, refColour);
    trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ab::refTrim, trimSlider);

    monitorPanel.addAndMakeVisible (matchLabel);
    matchLabel.setFont (makeFont (11.0f));
    matchLabel.setColour (juce::Label::textColourId, textDim);
    matchLabel.setJustificationType (juce::Justification::centredRight);

    setSize (editorWidth, editorHeight);

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto userArea = display->userArea;
        const double fitScale = juce::jlimit ((double) minUIScale, 1.0,
                                              juce::jmin ((double) userArea.getWidth() * 0.9 / (double) editorWidth,
                                                          (double) userArea.getHeight() * 0.9 / (double) editorHeight));

        if (fitScale < 1.0)
            setSize (juce::roundToInt ((float) editorWidth * (float) fitScale),
                     juce::roundToInt ((float) editorHeight * (float) fitScale));
    }

    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) editorWidth * minUIScale),
                     juce::roundToInt ((float) editorHeight * minUIScale),
                     2640, 1280);
    getConstrainer()->setFixedAspectRatio ((double) editorWidth / (double) editorHeight);

    updateFileLabels();
    startTimerHz (24);
}

ABmatchAudioProcessorEditor::~ABmatchAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void ABmatchAudioProcessorEditor::setMonitor (int source)
{
    if (auto* parameter = processor.apvts.getParameter (ab::monitor))
        parameter->setValueNotifyingHost (source > 0 ? 1.0f : 0.0f);
}

void ABmatchAudioProcessorEditor::chooseReference()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a reference track",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    juce::Component::SafePointer<ABmatchAudioProcessorEditor> safeThis (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr)
                                  return;

                              const auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              juce::String error;

                              if (! safeThis->processor.loadReference (file, error))
                                  juce::NativeMessageBox::showMessageBoxAsync (
                                      juce::MessageBoxIconType::WarningIcon, "ABmatch", error);

                              safeThis->updateFileLabels();
                          });
}

void ABmatchAudioProcessorEditor::updateFileLabels()
{
    const bool loaded = processor.isReferenceLoaded();

    fileNameLabel.setText (loaded ? processor.getReferenceName() : "No reference loaded",
                           juce::dontSendNotification);
    fileNameLabel.setColour (juce::Label::textColourId, loaded ? textBright : textDim);

    if (loaded)
    {
        const auto seconds = processor.getReferenceLengthSeconds();
        const int minutes = (int) (seconds / 60.0);
        const int secs = (int) std::floor (std::fmod (seconds, 60.0));

        fileInfoLabel.setText (juce::String (minutes) + ":"
                                 + juce::String (secs).paddedLeft ('0', 2)
                                 + "  ·  " + dbText (processor.getReferenceLufs(), " LUFS")
                                 + "  ·  " + dbText (processor.getReferencePeakDb(), " dBFS peak"),
                               juce::dontSendNotification);
    }
    else
    {
        fileInfoLabel.setText ("Load a finished track to compare against your mix", juce::dontSendNotification);
    }
}

//==============================================================================
void ABmatchAudioProcessorEditor::resized()
{
    const float scale = juce::jmin ((float) getWidth() / (float) editorWidth,
                                    (float) getHeight() / (float) editorHeight);

    const float offsetX = ((float) getWidth() - (float) editorWidth * scale) * 0.5f;
    const float offsetY = ((float) getHeight() - (float) editorHeight * scale) * 0.5f;

    contentTransform = juce::AffineTransform::scale (scale).translated (offsetX, offsetY);

    content.setBounds (0, 0, editorWidth, editorHeight);
    content.setTransform (contentTransform);

    mixButton.setBounds (880, 20, 200, 46);
    refButton.setBounds (1090, 20, 200, 46);

    referencePanel.setBounds (20, 88, 470, 196);
    monitorPanel.setBounds   (20, 300, 470, 252);
    meterPanel.setBounds     (510, 88, 292, 464);
    spectrumPanel.setBounds  (822, 88, 478, 464);

    // ---- Reference panel -------------------------------------------------
    fileNameLabel.setBounds (18, 38, 434, 24);
    fileInfoLabel.setBounds (18, 62, 434, 16);
    loadButton.setBounds    (18, 92, 152, 32);
    clearButton.setBounds   (178, 92, 78, 32);
    loopButton.setBounds    (264, 92, 78, 32);

    offsetLabel.setBounds      (18, 134, 64, 22);
    offsetSlider.setBounds     (86, 132, 196, 26);
    playheadButton.setBounds   (292, 132, 160, 26);

    // ---- Monitor panel ---------------------------------------------------
    loudnessButton.setBounds (18, 42, 200, 30);
    peakButton.setBounds     (18, 80, 200, 30);
    monoButton.setBounds     (18, 118, 200, 30);
    trimSlider.setBounds     (330, 40, 76, 92);
    trimLabel.setBounds      (300, 132, 136, 14);
    matchLabel.setBounds     (240, 158, 210, 22);

    // ---- Spectrum panel --------------------------------------------------
    showAButton.setBounds (spectrumPanel.getWidth() - 104, 8, 40, 20);
    showBButton.setBounds (spectrumPanel.getWidth() - 58, 8, 40, 20);
}

//==============================================================================
void ABmatchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f,
                                             bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colour (0xff1d2731));
    g.fillRect (0, 0, getWidth(), 1);

    juce::Graphics::ScopedSaveState savedState (g);
    g.addTransform (contentTransform);

    auto titleFont = makeFont (27.0f);
    titleFont.setExtraKerningFactor (0.14f);

    g.setColour (textBright);
    g.setFont (titleFont);
    g.drawText ("ABmatch", 28, 14, 420, 34, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (makeFont (10.0f));
    g.drawText ("REFERENCE MONITOR   ·   MIX A/B COMPARISON", 29, 48, 560, 16,
                juce::Justification::centredLeft);

    // Panel hints.
    g.setFont (makeFont (9.5f));
    g.setColour (textDim);
    g.drawText ("Playback runs from the start point with the transport; LOOP repeats the file.",
                38, 258, 440, 16, juce::Justification::centredLeft);

    g.drawText ("Matching applies a gain offset to the reference only.",
                38, 526, 440, 16, juce::Justification::centredLeft);

    g.drawText ("AURALIS AUDIO   ·   ABmatch v1.4", editorWidth - 340, editorHeight - 20, 320, 14,
                juce::Justification::centredRight);
}

//==============================================================================
void ABmatchAudioProcessorEditor::timerCallback()
{
    const bool monitoringRef = processor.isMonitoringReference();
    const bool hasRef = processor.isReferenceLoaded();

    mixButton.setToggleState (! monitoringRef, juce::dontSendNotification);
    refButton.setToggleState (monitoringRef && hasRef, juce::dontSendNotification);

    meterPanel.update (processor.getMeters(), monitoringRef,
                       processor.getReferenceLufs(), processor.getReferencePeakDb());
    meterPanel.repaint();

    const float gainDb = processor.getAppliedGainDb();
    matchLabel.setText ("MATCH  " + (std::abs (gainDb) < 0.05f
                                        ? juce::String ("0.0 dB")
                                        : (gainDb > 0.0f ? "+" : "") + juce::String (gainDb, 1) + " dB"),
                        juce::dontSendNotification);

    updateFileLabels();

    processor.getSpectrum (0, mixSpectrumValues.data());
    processor.getSpectrum (1, refSpectrumValues.data());

    spectrumPanel.update (mixSpectrumValues.data(), refSpectrumValues.data(),
                          showAButton.getToggleState(), showBButton.getToggleState(),
                          hasRef);
    spectrumPanel.repaint();
}
