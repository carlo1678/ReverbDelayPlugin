/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class ReverbDelayPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ReverbDelayPluginAudioProcessorEditor(ReverbDelayPluginAudioProcessor&);
    ~ReverbDelayPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setupSlider(juce::Slider& slider, juce::Label& label,
        const juce::String& labelText, const juce::String& suffix);

    // Custom LookAndFeel for pedal-style knobs
    class PedalKnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
            juce::Slider& slider) override
        {
            auto radius = juce::jmin(width / 2, height / 2) - 4.0f;
            auto centreX = x + width * 0.5f;
            auto centreY = y + height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            // Draw outer circle (knob body) - black with white outline
            g.setColour(juce::Colours::black);
            g.fillEllipse(rx, ry, rw, rw);

            g.setColour(juce::Colours::white);
            g.drawEllipse(rx, ry, rw, rw, 2.0f);

            // Draw indicator line (white line from center to edge)
            juce::Path p;
            auto pointerLength = radius * 0.7f;
            auto pointerThickness = 3.0f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

            g.setColour(juce::Colours::white);
            g.fillPath(p);
        }
    };

private:
    // Reference to the processor
    ReverbDelayPluginAudioProcessor& audioProcessor;

    // Time Mode selector
    juce::ComboBox timeModeBox;

    // Custom knob look and feel
    PedalKnobLookAndFeel pedalKnobLAF;

    // Sliders (knobs)
    juce::Slider mixSlider;
    juce::Slider delayTimeSlider;
    juce::Slider delayFeedbackSlider;

    // Toggle buttons
    juce::ToggleButton reverseDelayButton;
    juce::ToggleButton pingPongButton;

    // Labels
    juce::Label mixLabel;
    juce::Label delayTimeLabel;
    juce::Label delayFeedbackLabel;
    juce::Label reverseDelayLabel;
    juce::Label tempoSyncLabel;
    juce::Label timeModeLabel;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbDelayPluginAudioProcessorEditor)
};