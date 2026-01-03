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
    // Set window size
    setSize(600, 450);

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

    // Setup Feedback Slider (rotary knob)
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK", "");
    delayFeedbackAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_feedback", delayFeedbackSlider));


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
}



ReverbDelayPluginAudioProcessorEditor::~ReverbDelayPluginAudioProcessorEditor()
{
    // Reset LookAndFeel for all sliders (IMPORTANT!)
    mixSlider.setLookAndFeel(nullptr);
    delayTimeSlider.setLookAndFeel(nullptr);
    delayFeedbackSlider.setLookAndFeel(nullptr);
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
}

void ReverbDelayPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(30, 30);

    // Title space
    bounds.removeFromTop(60);
    bounds.removeFromTop(10);

    // Top row: Mix knob centered
    auto topRow = bounds.removeFromTop(120);
    mixSlider.setBounds(topRow.withSizeKeepingCentre(110, 110));

    bounds.removeFromTop(20);

    // Middle row: TIME and FEEDBACK knobs
    auto middleRow = bounds.removeFromTop(120);
    int knobWidth = middleRow.getWidth() / 2;

    delayTimeSlider.setBounds(middleRow.removeFromLeft(knobWidth).reduced(20));
    delayFeedbackSlider.setBounds(middleRow.reduced(20));

    bounds.removeFromTop(5);

    // Time Mode dropdown (under TIME knob, left-aligned)
    auto timeModeRow = bounds.removeFromTop(55);
    auto timeModeArea = timeModeRow.removeFromLeft(knobWidth);
    timeModeArea.removeFromTop(25); // Space for "MODE" label
    timeModeBox.setBounds(timeModeArea.withSizeKeepingCentre(120, 30));

    bounds.removeFromTop(10);

    // Bottom row: Controls - evenly spaced (2 buttons)
    auto bottomRow = bounds.removeFromTop(70);

    int buttonWidth = bottomRow.getWidth() / 2; // Divide into 2 equal sections

    // Ping Pong button (left half)
    auto pingPongButtonArea = bottomRow.removeFromLeft(buttonWidth);
    pingPongButton.setBounds(pingPongButtonArea.withSizeKeepingCentre(120, 28));

    // Reverse button (right half)
    reverseDelayButton.setBounds(bottomRow.withSizeKeepingCentre(120, 28));
}