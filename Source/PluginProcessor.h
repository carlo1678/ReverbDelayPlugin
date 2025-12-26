/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/DelayLine.h"


//==============================================================================
/**
*/
class ReverbDelayPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ReverbDelayPluginAudioProcessor();
    ~ReverbDelayPluginAudioProcessor() override;


    // Create parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
        // Parameter management
    juce::AudioProcessorValueTreeState parameters;

    // Parameter pointers for fast access
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* reverbDecayParam = nullptr;
    std::atomic<float>* reverbPreDelayParam = nullptr;
    std::atomic<float>* reverbSizeParam = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayPitchParam = nullptr;
    std::atomic<float>* reverseDelayParam = nullptr;

    // DSP Components
    DelayLine delayLineLeft;
    DelayLine delayLineRight;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbDelayPluginAudioProcessor)
};
