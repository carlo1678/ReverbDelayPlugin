/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReverbDelayPluginAudioProcessor::ReverbDelayPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
#else
    :
#endif
parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    // Get parameter pointers for fast access
    mixParam = parameters.getRawParameterValue("mix");
    reverbDecayParam = parameters.getRawParameterValue("reverb_decay");
    reverbPreDelayParam = parameters.getRawParameterValue("reverb_predelay");
    reverbSizeParam = parameters.getRawParameterValue("reverb_size");
    reverbDampingParam = parameters.getRawParameterValue("reverb_damping");
    delayTimeParam = parameters.getRawParameterValue("delay_time");
    delayFeedbackParam = parameters.getRawParameterValue("delay_feedback");
    delayPitchParam = parameters.getRawParameterValue("delay_pitch");
    reverseDelayParam = parameters.getRawParameterValue("reverse_delay");
}


ReverbDelayPluginAudioProcessor::~ReverbDelayPluginAudioProcessor()
{
}


//==============================================================================
const juce::String ReverbDelayPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ReverbDelayPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ReverbDelayPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ReverbDelayPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ReverbDelayPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ReverbDelayPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ReverbDelayPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ReverbDelayPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ReverbDelayPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void ReverbDelayPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ReverbDelayPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare delay lines (max 2 seconds)
    delayLineLeft.prepare(sampleRate, 2000);
    delayLineRight.prepare(sampleRate, 2000);
}


void ReverbDelayPluginAudioProcessor::releaseResources()
{
    // Reset delay lines
    delayLineLeft.reset();
    delayLineRight.reset();
}


#ifndef JucePlugin_PreferredChannelConfigurations
bool ReverbDelayPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ReverbDelayPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
        // Get parameter values
    float delayTime = delayTimeParam->load();
    float feedback = delayFeedbackParam->load();
    float mix = mixParam->load() / 100.0f; // Convert 0-100 to 0-1

    // Process left channel
    if (totalNumInputChannels > 0)
    {
        auto* leftChannel = buffer.getWritePointer(0);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = leftChannel[sample];
            float delayed = delayLineLeft.processSample(input, delayTime, feedback);

            // Mix dry and wet signal
            leftChannel[sample] = input * (1.0f - mix) + delayed * mix;
        }
    }

    // Process right channel
    if (totalNumInputChannels > 1)
    {
        auto* rightChannel = buffer.getWritePointer(1);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = rightChannel[sample];
            float delayed = delayLineRight.processSample(input, delayTime, feedback);

            // Mix dry and wet signal
            rightChannel[sample] = input * (1.0f - mix) + delayed * mix;
        }
    }

}

//==============================================================================
bool ReverbDelayPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ReverbDelayPluginAudioProcessor::createEditor()
{
    return new ReverbDelayPluginAudioProcessorEditor (*this);
}

//==============================================================================
void ReverbDelayPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ReverbDelayPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ReverbDelayPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mix (Dry/Wet balance)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix", 0.0f, 100.0f, 50.0f));

    // Reverb Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverb_decay", "Reverb Decay", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverb_predelay", "Reverb Pre-Delay", 0.0f, 100.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverb_size", "Reverb Size", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverb_damping", "Reverb Damping", 0.0f, 1.0f, 0.5f));

    // Delay Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_time", "Delay Time", 0.0f, 2000.0f, 500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_feedback", "Delay Feedback", 0.0f, 0.95f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_pitch", "Delay Pitch", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverse_delay", "Reverse Delay", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbDelayPluginAudioProcessor();
}
