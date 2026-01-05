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
    delayTimeParam = parameters.getRawParameterValue("delay_time");
    delayFeedbackParam = parameters.getRawParameterValue("delay_feedback");
    reverseDelayParam = parameters.getRawParameterValue("reverse_delay");
    tempoSyncParam = parameters.getRawParameterValue("tempo_sync");
    pingPongParam = parameters.getRawParameterValue("ping_pong");
    timeModeParam = parameters.getRawParameterValue("time_mode");
    lowCutParam = parameters.getRawParameterValue("low_cut");
    highCutParam = parameters.getRawParameterValue("high_cut");
    wowParam = parameters.getRawParameterValue("wow");
    flutterParam = parameters.getRawParameterValue("flutter");
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

    // Store sample rate for filter coefficient updates
    lastSampleRate = sampleRate;

    // Prepare filter chains
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;

    lowCutFilterLeft.prepare(spec);
    lowCutFilterRight.prepare(spec);
    highCutFilterLeft.prepare(spec);
    highCutFilterRight.prepare(spec);

    lowCutFilterLeft.reset();
    lowCutFilterRight.reset();
    highCutFilterLeft.reset();
    highCutFilterRight.reset();
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
    float feedback = delayFeedbackParam->load();
    float mix = mixParam->load() / 100.0f; // Convert 0-100 to 0-1
    bool reverseEnabled = reverseDelayParam->load() > 0.5f;
    bool pingPongEnabled = pingPongParam->load() > 0.5f;
    float lowCutFreq = lowCutParam->load();
    float highCutFreq = highCutParam->load();
    float wow = wowParam->load();
    float flutter = flutterParam->load();

    // Only update filter coefficients when parameters actually change
    // This prevents crackling/zipper noise from constant coefficient updates
    const float freqChangeThreshold = 0.1f; // Only update if changed by more than 0.1 Hz

    if (std::abs(lowCutFreq - lastLowCutFreq) > freqChangeThreshold)
    {
        auto lowCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(lastSampleRate, lowCutFreq);

        *lowCutFilterLeft.get<0>().coefficients = *lowCutCoefficients;
        *lowCutFilterLeft.get<1>().coefficients = *lowCutCoefficients;
        *lowCutFilterLeft.get<2>().coefficients = *lowCutCoefficients;
        *lowCutFilterRight.get<0>().coefficients = *lowCutCoefficients;
        *lowCutFilterRight.get<1>().coefficients = *lowCutCoefficients;
        *lowCutFilterRight.get<2>().coefficients = *lowCutCoefficients;

        lastLowCutFreq = lowCutFreq;
    }

    if (std::abs(highCutFreq - lastHighCutFreq) > freqChangeThreshold)
    {
        auto highCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(lastSampleRate, highCutFreq);

        *highCutFilterLeft.get<0>().coefficients = *highCutCoefficients;
        *highCutFilterLeft.get<1>().coefficients = *highCutCoefficients;
        *highCutFilterLeft.get<2>().coefficients = *highCutCoefficients;
        *highCutFilterRight.get<0>().coefficients = *highCutCoefficients;
        *highCutFilterRight.get<1>().coefficients = *highCutCoefficients;
        *highCutFilterRight.get<2>().coefficients = *highCutCoefficients;

        lastHighCutFreq = highCutFreq;
    }

    // Get time mode (0=Notes, 1=Time, 2=Triplet, 3=Dotted)
    int timeMode = static_cast<int>(timeModeParam->load());

    // Always tempo sync except in TIME mode
    bool tempoSyncEnabled = (timeMode != 1);

    // Get note division from TIME knob (0-4)
    int noteDivision = static_cast<int>(delayTimeParam->load());

    // Calculate delay time
    float delayTime = 0.0f;

    if (timeMode == 1) // TIME mode - use milliseconds directly
    {
        // In TIME mode, map note divisions to millisecond ranges
        // 0=100ms, 1=400ms, 2=800ms, 3=1400ms, 4=2000ms
        switch (noteDivision)
        {
        case 0: delayTime = 100.0f; break;
        case 1: delayTime = 400.0f; break;
        case 2: delayTime = 800.0f; break;
        case 3: delayTime = 1400.0f; break;
        case 4: delayTime = 2000.0f; break;
        default: delayTime = 800.0f; break;
        }
    }
    else // NOTES, TRIPLET, or DOTTED modes - calculate based on tempo
    {
        double bpm = 120.0; // Default tempo when not synced

        if (tempoSyncEnabled)
        {
            // Get tempo from host
            auto playHead = getPlayHead();
            if (playHead != nullptr)
            {
                juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
                if (posInfo.hasValue() && posInfo->getBpm().hasValue())
                {
                    bpm = *posInfo->getBpm();
                }
            }
        }

        // Calculate base delay time
        double beatsPerSecond = bpm / 60.0;
        double millisecondsPerBeat = 1000.0 / beatsPerSecond;

        float baseTime = 0.0f;
        switch (noteDivision)
        {
        case 0: baseTime = millisecondsPerBeat / 4.0f; break;  // 1/16 note
        case 1: baseTime = millisecondsPerBeat / 2.0f; break;  // 1/8 note
        case 2: baseTime = millisecondsPerBeat; break;         // 1/4 note
        case 3: baseTime = millisecondsPerBeat * 2.0f; break;  // 1/2 note
        case 4: baseTime = millisecondsPerBeat * 4.0f; break;  // Whole note
        default: baseTime = millisecondsPerBeat; break;
        }

        // Apply time mode multiplier
        if (timeMode == 0) // NOTES mode
        {
            delayTime = baseTime;
        }
        else if (timeMode == 2) // TRIPLET mode (2/3 of base)
        {
            delayTime = baseTime * (2.0f / 3.0f);
        }
        else if (timeMode == 3) // DOTTED mode (1.5x base)
        {
            delayTime = baseTime * 1.5f;
        }
    }

    // Process stereo channels
    if (totalNumInputChannels >= 2)
    {
        auto* leftChannel = buffer.getWritePointer(0);
        auto* rightChannel = buffer.getWritePointer(1);

        if (pingPongEnabled)
        {
            // Ping pong mode: delays bounce between left and right
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float leftInput = leftChannel[sample];
                float rightInput = rightChannel[sample];

                // Process delays (left delay goes to right, right delay goes to left)
                float leftDelayed = delayLineLeft.processSample(rightInput, delayTime, feedback, 0.0f, reverseEnabled, wow, flutter);
                float rightDelayed = delayLineRight.processSample(leftInput, delayTime, feedback, 0.0f, reverseEnabled, wow, flutter);

                // Apply filters to delayed signal
                leftDelayed = lowCutFilterLeft.get<0>().processSample(leftDelayed);
                leftDelayed = lowCutFilterLeft.get<1>().processSample(leftDelayed);
                leftDelayed = lowCutFilterLeft.get<2>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<0>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<1>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<2>().processSample(leftDelayed);

                rightDelayed = lowCutFilterRight.get<0>().processSample(rightDelayed);
                rightDelayed = lowCutFilterRight.get<1>().processSample(rightDelayed);
                rightDelayed = lowCutFilterRight.get<2>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<0>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<1>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<2>().processSample(rightDelayed);

                // Mix dry and wet (swapped for ping pong)
                leftChannel[sample] = leftInput * (1.0f - mix) + leftDelayed * mix;
                rightChannel[sample] = rightInput * (1.0f - mix) + rightDelayed * mix;
            }
        }
        else
        {
            // Normal mode: delays stay on same channel
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float leftInput = leftChannel[sample];
                float rightInput = rightChannel[sample];

                float leftDelayed = delayLineLeft.processSample(leftInput, delayTime, feedback, 0.0f, reverseEnabled, wow, flutter);
                float rightDelayed = delayLineRight.processSample(rightInput, delayTime, feedback, 0.0f, reverseEnabled, wow, flutter);

                // Apply filters to delayed signal
                leftDelayed = lowCutFilterLeft.get<0>().processSample(leftDelayed);
                leftDelayed = lowCutFilterLeft.get<1>().processSample(leftDelayed);
                leftDelayed = lowCutFilterLeft.get<2>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<0>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<1>().processSample(leftDelayed);
                leftDelayed = highCutFilterLeft.get<2>().processSample(leftDelayed);

                rightDelayed = lowCutFilterRight.get<0>().processSample(rightDelayed);
                rightDelayed = lowCutFilterRight.get<1>().processSample(rightDelayed);
                rightDelayed = lowCutFilterRight.get<2>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<0>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<1>().processSample(rightDelayed);
                rightDelayed = highCutFilterRight.get<2>().processSample(rightDelayed);

                leftChannel[sample] = leftInput * (1.0f - mix) + leftDelayed * mix;
                rightChannel[sample] = rightInput * (1.0f - mix) + rightDelayed * mix;
            }
        }
    }
    else if (totalNumInputChannels > 0)
    {
        // Mono input - process left channel only
        auto* leftChannel = buffer.getWritePointer(0);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = leftChannel[sample];
            float delayed = delayLineLeft.processSample(input, delayTime, feedback, 0.0f, reverseEnabled, wow, flutter);

            // Apply filters to delayed signal
            delayed = lowCutFilterLeft.get<0>().processSample(delayed);
            delayed = lowCutFilterLeft.get<1>().processSample(delayed);
            delayed = lowCutFilterLeft.get<2>().processSample(delayed);
            delayed = highCutFilterLeft.get<0>().processSample(delayed);
            delayed = highCutFilterLeft.get<1>().processSample(delayed);
            delayed = highCutFilterLeft.get<2>().processSample(delayed);

            leftChannel[sample] = input * (1.0f - mix) + delayed * mix;
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

    // Delay Time (note divisions)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "delay_time", "Time",
        juce::StringArray{ "1/16", "1/8", "1/4", "1/2", "Whole" }, 2)); // Default to 1/4

    // Time Mode (Notes, Time, Triplet, Dotted)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "time_mode", "Time Mode",
        juce::StringArray{ "Notes", "Time", "Triplet", "Dotted" }, 0)); // Default to Notes

    // Delay Feedback
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_feedback", "Delay Feedback", 0.0f, 0.99f, 0.3f));

    // Tempo Sync On/Off
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "tempo_sync", "Tempo Sync", false));


    // Reverse Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverse_delay", "Reverse Delay", 0.0f, 1.0f, 0.0f));

    // Ping Pong (stereo bouncing delay)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "ping_pong", "Ping Pong", false));

    // Low Cut (highpass filter) - 20 Hz to 1000 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "low_cut", "Low Cut",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.3f),
        20.0f));

    // High Cut (lowpass filter) - 1000 Hz to 20000 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "high_cut", "High Cut",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.3f),
        20000.0f));

    // Wow (slow pitch modulation) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "wow", "Wow", 0.0f, 100.0f, 0.0f));

    // Flutter (fast pitch modulation) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "flutter", "Flutter", 0.0f, 100.0f, 0.0f));

    return { params.begin(), params.end() };
}



//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbDelayPluginAudioProcessor();
}

//==============================================================================
//==============================================================================
// Preset Management

juce::StringArray ReverbDelayPluginAudioProcessor::getPresetNames()
{
    return {
        "Telephone",
        "Underwater",
        "Tape",
        "Radio"
    };
}

int ReverbDelayPluginAudioProcessor::getNumPresets()
{
    return getPresetNames().size();
}

void ReverbDelayPluginAudioProcessor::loadPreset(int presetIndex)
{
    switch (presetIndex)
    {
    case 0: // Telephone - Classic telephone effect with narrow frequency range
        parameters.getParameter("mix")->setValueNotifyingHost(0.40f); // 40%
        parameters.getParameter("delay_time")->setValueNotifyingHost(0.5f); // 1/4 note (index 2)
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode (index 0)
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.30f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.40f); // ~400 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.15f); // ~3000 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.0f); // 0%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.0f); // 0%
        break;

    case 1: // Underwater - Deep, muffled underwater sound
        parameters.getParameter("mix")->setValueNotifyingHost(0.60f); // 60%
        parameters.getParameter("delay_time")->setValueNotifyingHost(0.5f); // 1/4 note (index 2)
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.65f); // High feedback for swirly effect
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.0f); // 20 Hz (minimum - keep bass)
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.05f); // ~1500 Hz (heavy high cut)
        parameters.getParameter("wow")->setValueNotifyingHost(0.30f); // 30%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.20f); // 20%
        break;

    case 2: // Tape - Classic cassette tape feel with wow and flutter
        parameters.getParameter("mix")->setValueNotifyingHost(0.50f); // 50%
        parameters.getParameter("delay_time")->setValueNotifyingHost(0.5f); // 1/4 note (index 2)
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.33f); // Time mode (index 1)
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.50f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(0.0f); // Off for vintage feel
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.06f); // ~80 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.40f); // ~8000 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.40f); // 40% (tape warble)
        parameters.getParameter("flutter")->setValueNotifyingHost(0.30f); // 30%
        break;

    case 3: // Radio - Vintage radio broadcast sound
        parameters.getParameter("mix")->setValueNotifyingHost(0.35f); // 35%
        parameters.getParameter("delay_time")->setValueNotifyingHost(0.5f); // 1/4 note (index 2)
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.20f); // Low feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.60f); // ~600 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.13f); // ~2800 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.10f); // 10% (slight)
        parameters.getParameter("flutter")->setValueNotifyingHost(0.0f); // 0%
        break;

    default:
        break;
    }
}