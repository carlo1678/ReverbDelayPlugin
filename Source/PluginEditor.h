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

    // Preset selector
    juce::ComboBox presetBox;

    // Custom knob look and feel
    PedalKnobLookAndFeel pedalKnobLAF;

    // Sliders (knobs)
    juce::Slider mixSlider;
    juce::Slider delayTimeSlider;
    juce::Slider delayFeedbackSlider;
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider wowSlider;
    juce::Slider flutterSlider;
    juce::Slider noiseSlider;
    juce::Slider phaserMixSlider;
    juce::Slider phaserSpeedSlider;
    juce::Slider underwaterMixSlider;
    juce::Slider mechanicalNoiseSlider;
    juce::Slider radioNoiseSlider;
    juce::Slider pendulumSpeedSlider;

    // Toggle buttons
    juce::ToggleButton reverseDelayButton;
    juce::ToggleButton pingPongButton;
    juce::ToggleButton pendulumPanButton;

    // Reset button
    juce::TextButton resetButton;

    // Labels
    juce::Label mixLabel;
    juce::Label delayTimeLabel;
    juce::Label delayFeedbackLabel;
    juce::Label reverseDelayLabel;
    juce::Label tempoSyncLabel;
    juce::Label timeModeLabel;
    juce::Label lowCutLabel;
    juce::Label highCutLabel;
    juce::Label wowLabel;
    juce::Label flutterLabel;
    juce::Label modLabel;  // "MOD" title for the mod box
    juce::Label presetEfxLabel;  // "Preset Specific EFX" title for the preset effects box
    juce::Label presetLabel;
    juce::Label noiseLabel;
    juce::Label phaserMixLabel;
    juce::Label phaserSpeedLabel;
    juce::Label underwaterMixLabel;
    juce::Label mechanicalNoiseLabel;
    juce::Label radioNoiseLabel;
    juce::Label pendulumSpeedLabel;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowCutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highCutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaserMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaserSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> underwaterMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mechanicalNoiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> radioNoiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pendulumPanAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pendulumSpeedAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbDelayPluginAudioProcessorEditor)
};