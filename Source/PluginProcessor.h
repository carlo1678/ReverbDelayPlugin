/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/DelayLine.h"
#include "DSP/ReverbEngine.h"


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

    // Preset management
    void loadPreset(int presetIndex);
    juce::StringArray getPresetNames();
    int getNumPresets();

    // Parameter management (needs to be public for Editor access)
    juce::AudioProcessorValueTreeState parameters;


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


    // Parameter pointers for fast access
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* reverseDelayParam = nullptr;
    std::atomic<float>* tempoSyncParam = nullptr;
    std::atomic<float>* pingPongParam = nullptr;
    std::atomic<float>* timeModeParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* pendulumPanParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* phaserMixParam = nullptr;
    std::atomic<float>* phaserSpeedParam = nullptr;
    std::atomic<float>* underwaterMixParam = nullptr;
    std::atomic<float>* mechanicalNoiseParam = nullptr;

    // DSP Components
    DelayLine delayLineLeft;
    DelayLine delayLineRight;

    // Filter state for delayed signal (18 dB/octave = 3 poles)
    juce::dsp::ProcessorChain<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Filter<float>> lowCutFilterLeft, lowCutFilterRight;

    juce::dsp::ProcessorChain<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Filter<float>> highCutFilterLeft, highCutFilterRight;

    double lastSampleRate = 44100.0;

    // Track last filter frequencies to avoid unnecessary updates
    float lastLowCutFreq = 20.0f;
    float lastHighCutFreq = 20000.0f;

    // Pendulum panning state
    float pendulumPhase = 0.0f;

    // Phaser state
    float phaserPhase = 0.0f;

    // Telephone noise sample buffer and playback position
    juce::AudioBuffer<float> telephoneNoiseSample;
    int telephoneNoiseReadPos = 0;

    // Underwater sound effect buffer and playback position
    juce::AudioBuffer<float> underwaterSoundSample;
    int underwaterSoundReadPos = 0;

    // Mechanical noise sample buffer and playback position
    juce::AudioBuffer<float> mechanicalNoiseSample;
    int mechanicalNoiseReadPos = 0;

    // Envelope following for audio overlays
    float telephoneEnvelope = 0.0f;
    float underwaterEnvelope = 0.0f;
    float mechanicalEnvelope = 0.0f;
    const float envelopeAttack = 0.99f;  // Fast attack
    float envelopeRelease = 0.9995f;     // Slow release (will be adjusted by feedback)

    // Helper methods to load audio samples
    void loadTelephoneNoise();
    void loadUnderwaterSound();
    void loadMechanicalNoise();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbDelayPluginAudioProcessor)
};
