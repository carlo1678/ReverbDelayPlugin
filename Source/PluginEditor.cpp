/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void ReverbDelayPluginAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label,
    const juce::String& labelText,
    const juce::String& suffix)
{
    // ROTARY knob style (like a real pedal)
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

    // 270-degree rotation range
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
        juce::MathConstants<float>::pi * 2.8f,
        true);

    // APPLY CUSTOM PEDAL KNOB LOOK (THIS WAS MISSING!)
    slider.setLookAndFeel(&pedalKnobLAF);

    // Black and white text colors
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);

    slider.setTextValueSuffix(" " + suffix);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(label);
}

//==============================================================================
ReverbDelayPluginAudioProcessorEditor::ReverbDelayPluginAudioProcessorEditor(ReverbDelayPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set window size (increased to accommodate Mod box)
    setSize(750, 600);

    // Setup Mix Slider (rotary knob)
    setupSlider(mixSlider, mixLabel, "MIX", "%");
    mixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "mix", mixSlider));

    // Setup Delay Time Slider (rotary knob)
    setupSlider(delayTimeSlider, delayTimeLabel, "TIME", "");
    delayTimeAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_time", delayTimeSlider));

    // Setup Delay Time Slider (rotary knob) - now shows note divisions
    setupSlider(delayTimeSlider, delayTimeLabel, "TIME", "");
    delayTimeAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_time", delayTimeSlider));

    // Configure to show note division text instead of numbers
    delayTimeSlider.textFromValueFunction = [](double value)
        {
            int index = static_cast<int>(value);
            switch (index)
            {
            case 0: return juce::String("1/16");
            case 1: return juce::String("1/8");
            case 2: return juce::String("1/4");
            case 3: return juce::String("1/2");
            case 4: return juce::String("Whole");
            default: return juce::String("1/4");
            }
        };

    // Setup Time Mode Dropdown
    timeModeBox.addItem("Notes", 1);
    timeModeBox.addItem("Time", 2);
    timeModeBox.addItem("Triplet", 3);
    timeModeBox.addItem("Dotted", 4);
    timeModeBox.setSelectedId(1); // Default to Notes
    timeModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    timeModeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    timeModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    timeModeBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(timeModeBox);
    timeModeAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        audioProcessor.parameters, "time_mode", timeModeBox));

    timeModeLabel.setText("MODE", juce::dontSendNotification);
    timeModeLabel.setJustificationType(juce::Justification::centred);
    timeModeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeModeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    timeModeLabel.attachToComponent(&timeModeBox, false);
    addAndMakeVisible(timeModeLabel);

    // Setup Preset selector
    auto presetNames = audioProcessor.getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetBox.addItem(presetNames[i], i + 1); // IDs start at 1
    }
    presetBox.setSelectedId(0); // No preset selected by default
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    presetBox.onChange = [this]
    {
        int selectedId = presetBox.getSelectedId();
        if (selectedId > 0)
        {
            audioProcessor.loadPreset(selectedId - 1); // Convert ID back to index
        }
    };
    addAndMakeVisible(presetBox);

    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    presetLabel.attachToComponent(&presetBox, false);
    addAndMakeVisible(presetLabel);

    // Setup Feedback Slider (rotary knob)
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK", "");
    delayFeedbackAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_feedback", delayFeedbackSlider));

    // Setup Low Cut Slider (rotary knob)
    setupSlider(lowCutSlider, lowCutLabel, "LOW CUT", " Hz");
    lowCutAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "low_cut", lowCutSlider));

    // Setup High Cut Slider (rotary knob)
    setupSlider(highCutSlider, highCutLabel, "HIGH CUT", " Hz");
    highCutAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "high_cut", highCutSlider));

    // Setup Wow Slider (rotary knob)
    setupSlider(wowSlider, wowLabel, "WOW", "%");
    wowAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "wow", wowSlider));

    // Setup Flutter Slider (rotary knob)
    setupSlider(flutterSlider, flutterLabel, "FLUTTER", "%");
    flutterAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "flutter", flutterSlider));

    // Setup MOD label
    modLabel.setText("MOD", juce::dontSendNotification);
    modLabel.setJustificationType(juce::Justification::centred);
    modLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    modLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(modLabel);

    // Setup Reverse Button
    reverseDelayButton.setButtonText("REVERSE");
    reverseDelayButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    reverseDelayButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    reverseDelayButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(reverseDelayButton);
    reverseDelayAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "reverse_delay", reverseDelayButton));

    reverseDelayAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "reverse_delay", reverseDelayButton));

    // Setup Ping Pong Button
    pingPongButton.setButtonText("PING PONG");
    pingPongButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    pingPongButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    pingPongButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(pingPongButton);
    pingPongAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "ping_pong", pingPongButton));

    // Setup Pendulum Pan Button
    pendulumPanButton.setButtonText("PENDULUM PAN");
    pendulumPanButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    pendulumPanButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    pendulumPanButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(pendulumPanButton);
    pendulumPanAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "pendulum_pan", pendulumPanButton));
}



ReverbDelayPluginAudioProcessorEditor::~ReverbDelayPluginAudioProcessorEditor()
{
    // Reset LookAndFeel for all sliders (IMPORTANT!)
    mixSlider.setLookAndFeel(nullptr);
    delayTimeSlider.setLookAndFeel(nullptr);
    delayFeedbackSlider.setLookAndFeel(nullptr);
    lowCutSlider.setLookAndFeel(nullptr);
    highCutSlider.setLookAndFeel(nullptr);
    wowSlider.setLookAndFeel(nullptr);
    flutterSlider.setLookAndFeel(nullptr);
}

//==============================================================================
void ReverbDelayPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Black background
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds();

    // Draw title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText("The Lockboxx Effect", bounds.removeFromTop(60), juce::Justification::centred);

    // Draw MOD box border
    // Calculate mod box position (same logic as in resized())
    auto modBoxBounds = getLocalBounds();
    modBoxBounds.reduce(30, 30);
    modBoxBounds.removeFromTop(60 + 10 + 120 + 20); // Skip title, top row spacing

    auto middleRow = modBoxBounds.removeFromTop(180);
    int middleSectionWidth = middleRow.getWidth() / 3;
    middleRow.removeFromLeft(middleSectionWidth); // Skip Low Cut section

    auto modBoxSection = middleRow.removeFromLeft(middleSectionWidth);
    modBoxSection.reduce(10, 10); // Add some padding

    // Draw rounded rectangle border for MOD box
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(modBoxSection.toFloat(), 8.0f, 2.0f);
}

void ReverbDelayPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(30, 30);

    // Title space
    auto titleArea = bounds.removeFromTop(60);

    // Preset selector in top right corner
    auto presetArea = titleArea.removeFromRight(200);
    presetArea.removeFromTop(25); // Space for "PRESET" label
    presetBox.setBounds(presetArea.withSizeKeepingCentre(180, 30));

    bounds.removeFromTop(10);

    // Top row: TIME - MIX - FEEDBACK
    auto topRow = bounds.removeFromTop(120);
    int topSectionWidth = topRow.getWidth() / 3;

    // TIME knob (left section)
    delayTimeSlider.setBounds(topRow.removeFromLeft(topSectionWidth).reduced(20));

    // MIX knob (center section)
    auto mixSection = topRow.removeFromLeft(topSectionWidth);
    mixSlider.setBounds(mixSection.withSizeKeepingCentre(110, 110));

    // FEEDBACK knob (right section)
    delayFeedbackSlider.setBounds(topRow.reduced(20));

    bounds.removeFromTop(20);

    // Middle row: Low Cut - MOD BOX - High Cut
    auto middleRow = bounds.removeFromTop(180);
    int middleSectionWidth = middleRow.getWidth() / 3;

    // Low Cut knob (left section)
    auto lowCutSection = middleRow.removeFromLeft(middleSectionWidth);
    lowCutSlider.setBounds(lowCutSection.removeFromTop(120).reduced(20));

    // MOD BOX (center section)
    auto modBoxSection = middleRow.removeFromLeft(middleSectionWidth);

    // MOD title label at top of box
    modLabel.setBounds(modBoxSection.removeFromTop(30));

    // Mod knobs side by side inside the box
    auto modKnobsArea = modBoxSection.removeFromTop(110);
    int modKnobWidth = modKnobsArea.getWidth() / 2;

    // Wow knob (left side of mod box)
    wowSlider.setBounds(modKnobsArea.removeFromLeft(modKnobWidth).reduced(10));

    // Flutter knob (right side of mod box)
    flutterSlider.setBounds(modKnobsArea.reduced(10));

    // High Cut knob (right section)
    auto highCutSection = middleRow;
    highCutSlider.setBounds(highCutSection.removeFromTop(120).reduced(20));

    bounds.removeFromTop(10);

    // MODE dropdown - centered below mod box
    auto modeRow = bounds.removeFromTop(55);
    modeRow.removeFromTop(25); // Space for "MODE" label
    timeModeBox.setBounds(modeRow.withSizeKeepingCentre(120, 30));

    bounds.removeFromTop(10);

    // Bottom row: PING PONG - PENDULUM PAN - REVERSE
    auto bottomRow = bounds.removeFromTop(70);
    int buttonWidth = bottomRow.getWidth() / 3;

    // Ping Pong button (left third)
    auto pingPongButtonArea = bottomRow.removeFromLeft(buttonWidth);
    pingPongButton.setBounds(pingPongButtonArea.withSizeKeepingCentre(120, 28));

    // Pendulum Pan button (center third)
    auto pendulumPanButtonArea = bottomRow.removeFromLeft(buttonWidth);
    pendulumPanButton.setBounds(pendulumPanButtonArea.withSizeKeepingCentre(140, 28));

    // Reverse button (right third)
    reverseDelayButton.setBounds(bottomRow.withSizeKeepingCentre(120, 28));
}