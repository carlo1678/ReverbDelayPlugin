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
    pendulumPanParam = parameters.getRawParameterValue("pendulum_pan");
    noiseParam = parameters.getRawParameterValue("noise");
    phaserMixParam = parameters.getRawParameterValue("phaser_mix");
    phaserSpeedParam = parameters.getRawParameterValue("phaser_speed");
    underwaterMixParam = parameters.getRawParameterValue("underwater_mix");
    mechanicalNoiseParam = parameters.getRawParameterValue("mechanical_noise");

    // Load audio samples
    loadTelephoneNoise();
    loadUnderwaterSound();
    loadMechanicalNoise();
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
    bool pendulumPanEnabled = pendulumPanParam->load() > 0.5f;
    float noise = noiseParam->load();
    float phaserMix = phaserMixParam->load() / 100.0f; // Convert 0-100 to 0-1
    float phaserSpeed = phaserSpeedParam->load();
    float underwaterMix = underwaterMixParam->load() / 100.0f; // Convert 0-100 to 0-1
    float mechanicalNoise = mechanicalNoiseParam->load(); // 0-100 range

    // Calculate dynamic envelope release based on feedback
    // Higher feedback = longer release time for overlay sounds
    // Map feedback (0-0.99) to release coefficient (0.995-0.9998)
    envelopeRelease = 0.995f + (feedback * 0.0048f);

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

    // Get delay time value (0-15000)
    float delayTimeValue = delayTimeParam->load();

    // Calculate delay time
    float delayTime = 0.0f;

    if (timeMode == 1) // TIME mode - use milliseconds directly
    {
        delayTime = delayTimeValue; // Direct milliseconds (0-15000)
    }
    else // NOTES, TRIPLET, or DOTTED modes - calculate based on tempo
    {
        // Map continuous value (0-15000) to note division (0-4)
        int noteDivision = static_cast<int>(delayTimeValue / 3000.0f);
        if (noteDivision > 4) noteDivision = 4;

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

    // Pendulum Panning: Calculate frequency for BPM-synced auto-pan
    // One full cycle (left->right->left) = 2 bars = 8 beats
    float pendulumFreq = 0.0f;
    if (pendulumPanEnabled)
    {
        double bpm = 120.0; // Default tempo
        auto playHead = getPlayHead();
        if (playHead != nullptr)
        {
            juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
            if (posInfo.hasValue() && posInfo->getBpm().hasValue())
            {
                bpm = *posInfo->getBpm();
            }
        }

        // Calculate frequency: 1 cycle per 2 bars = 1 cycle per 8 beats
        double beatsPerSecond = bpm / 60.0;
        pendulumFreq = static_cast<float>(beatsPerSecond / 8.0); // Hz for one cycle per 2 bars
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

                // Apply pendulum panning if enabled (even in ping-pong mode)
                if (pendulumPanEnabled)
                {
                    // Update pendulum phase
                    pendulumPhase += (2.0f * juce::MathConstants<float>::pi * pendulumFreq) / static_cast<float>(lastSampleRate);
                    if (pendulumPhase >= 2.0f * juce::MathConstants<float>::pi)
                        pendulumPhase -= 2.0f * juce::MathConstants<float>::pi;

                    // Calculate pan position using sine wave (-1 to +1)
                    float panPosition = std::sin(pendulumPhase);

                    // Convert pan position to stereo gains (constant power panning)
                    float angle = (panPosition + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                    float leftGain = std::cos(angle);
                    float rightGain = std::sin(angle);

                    // Apply panning to delayed signal
                    leftDelayed *= leftGain;
                    rightDelayed *= rightGain;
                }

                // Apply noise/static (telephone effect) using loaded audio sample
                if (noise > 0.0f && telephoneNoiseSample.getNumSamples() > 0)
                {
                    float noiseAmount = noise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update telephone envelope with attack/release
                    if (delayedLevel > telephoneEnvelope)
                        telephoneEnvelope = delayedLevel + envelopeAttack * (telephoneEnvelope - delayedLevel);
                    else
                        telephoneEnvelope = delayedLevel + envelopeRelease * (telephoneEnvelope - delayedLevel);

                    // Read from telephone noise buffer (loop if necessary)
                    int noiseChannel = telephoneNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float noiseSample = telephoneNoiseSample.getSample(noiseChannel, telephoneNoiseReadPos);

                    // Apply noise amount scaling and envelope
                    noiseSample *= noiseAmount * telephoneEnvelope;

                    leftDelayed += noiseSample;
                    rightDelayed += noiseSample;

                    // Increment and wrap read position
                    telephoneNoiseReadPos++;
                    if (telephoneNoiseReadPos >= telephoneNoiseSample.getNumSamples())
                        telephoneNoiseReadPos = 0;
                }

                // Apply phaser effect (amplitude modulation for call fading effect)
                if (phaserMix > 0.0f)
                {
                    // Update phaser phase
                    phaserPhase += (2.0f * juce::MathConstants<float>::pi * phaserSpeed) / static_cast<float>(lastSampleRate);
                    if (phaserPhase >= 2.0f * juce::MathConstants<float>::pi)
                        phaserPhase -= 2.0f * juce::MathConstants<float>::pi;

                    // Generate LFO (0.5 to 1.0 range for fading in/out effect)
                    float phaserLFO = 0.5f + (std::sin(phaserPhase) * 0.5f);

                    // Apply phaser to delayed signal
                    leftDelayed = leftDelayed * (1.0f - phaserMix) + (leftDelayed * phaserLFO * phaserMix);
                    rightDelayed = rightDelayed * (1.0f - phaserMix) + (rightDelayed * phaserLFO * phaserMix);
                }

                // Layer underwater sound effect (underwater preset only)
                if (underwaterMix > 0.0f && underwaterSoundSample.getNumSamples() > 0)
                {
                    // Calculate signal level from input signal (average of left and right)
                    float inputLevel = (std::abs(leftInput) + std::abs(rightInput)) * 0.5f;

                    // Update underwater envelope with attack/release
                    if (inputLevel > underwaterEnvelope)
                        underwaterEnvelope = inputLevel + envelopeAttack * (underwaterEnvelope - inputLevel);
                    else
                        underwaterEnvelope = inputLevel + envelopeRelease * (underwaterEnvelope - inputLevel);

                    // Read from underwater sound buffer (loop if necessary)
                    int underwaterChannel = underwaterSoundSample.getNumChannels() > 0 ? 0 : 0;
                    float underwaterSample = underwaterSoundSample.getSample(underwaterChannel, underwaterSoundReadPos);

                    // Apply envelope to underwater sound
                    underwaterSample *= underwaterEnvelope;

                    // Layer underwater sound with the input (not just delayed signal)
                    // This creates the underwater ambience effect
                    leftInput += underwaterSample * underwaterMix;
                    rightInput += underwaterSample * underwaterMix;

                    // Increment and wrap read position
                    underwaterSoundReadPos++;
                    if (underwaterSoundReadPos >= underwaterSoundSample.getNumSamples())
                        underwaterSoundReadPos = 0;
                }

                // Layer mechanical noise (tape preset only)
                if (mechanicalNoise > 0.0f && mechanicalNoiseSample.getNumSamples() > 0)
                {
                    float mechanicalAmount = mechanicalNoise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update mechanical envelope with attack/release
                    if (delayedLevel > mechanicalEnvelope)
                        mechanicalEnvelope = delayedLevel + envelopeAttack * (mechanicalEnvelope - delayedLevel);
                    else
                        mechanicalEnvelope = delayedLevel + envelopeRelease * (mechanicalEnvelope - delayedLevel);

                    // Read from mechanical noise buffer (loop if necessary)
                    int mechanicalChannel = mechanicalNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float mechanicalSample = mechanicalNoiseSample.getSample(mechanicalChannel, mechanicalNoiseReadPos);

                    // Apply mechanical noise amount and envelope
                    mechanicalSample *= mechanicalAmount * mechanicalEnvelope;

                    leftDelayed += mechanicalSample;
                    rightDelayed += mechanicalSample;

                    // Increment and wrap read position
                    mechanicalNoiseReadPos++;
                    if (mechanicalNoiseReadPos >= mechanicalNoiseSample.getNumSamples())
                        mechanicalNoiseReadPos = 0;
                }

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

                // Apply pendulum panning if enabled
                if (pendulumPanEnabled)
                {
                    // Update pendulum phase
                    pendulumPhase += (2.0f * juce::MathConstants<float>::pi * pendulumFreq) / static_cast<float>(lastSampleRate);
                    if (pendulumPhase >= 2.0f * juce::MathConstants<float>::pi)
                        pendulumPhase -= 2.0f * juce::MathConstants<float>::pi;

                    // Calculate pan position using sine wave (-1 to +1)
                    float panPosition = std::sin(pendulumPhase);

                    // Convert pan position to stereo gains (constant power panning)
                    // panPosition: -1 = full left, 0 = center, +1 = full right
                    float angle = (panPosition + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // Map to 0 to pi/2
                    float leftGain = std::cos(angle);
                    float rightGain = std::sin(angle);

                    // Apply panning to delayed signal
                    leftDelayed *= leftGain;
                    rightDelayed *= rightGain;
                }

                // Apply noise/static (telephone effect) using loaded audio sample
                if (noise > 0.0f && telephoneNoiseSample.getNumSamples() > 0)
                {
                    float noiseAmount = noise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update telephone envelope with attack/release
                    if (delayedLevel > telephoneEnvelope)
                        telephoneEnvelope = delayedLevel + envelopeAttack * (telephoneEnvelope - delayedLevel);
                    else
                        telephoneEnvelope = delayedLevel + envelopeRelease * (telephoneEnvelope - delayedLevel);

                    // Read from telephone noise buffer (loop if necessary)
                    int noiseChannel = telephoneNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float noiseSample = telephoneNoiseSample.getSample(noiseChannel, telephoneNoiseReadPos);

                    // Apply noise amount scaling and envelope
                    noiseSample *= noiseAmount * telephoneEnvelope;

                    leftDelayed += noiseSample;
                    rightDelayed += noiseSample;

                    // Increment and wrap read position
                    telephoneNoiseReadPos++;
                    if (telephoneNoiseReadPos >= telephoneNoiseSample.getNumSamples())
                        telephoneNoiseReadPos = 0;
                }

                // Apply phaser effect (amplitude modulation for call fading effect)
                if (phaserMix > 0.0f)
                {
                    // Update phaser phase
                    phaserPhase += (2.0f * juce::MathConstants<float>::pi * phaserSpeed) / static_cast<float>(lastSampleRate);
                    if (phaserPhase >= 2.0f * juce::MathConstants<float>::pi)
                        phaserPhase -= 2.0f * juce::MathConstants<float>::pi;

                    // Generate LFO (0.5 to 1.0 range for fading in/out effect)
                    float phaserLFO = 0.5f + (std::sin(phaserPhase) * 0.5f);

                    // Apply phaser to delayed signal
                    leftDelayed = leftDelayed * (1.0f - phaserMix) + (leftDelayed * phaserLFO * phaserMix);
                    rightDelayed = rightDelayed * (1.0f - phaserMix) + (rightDelayed * phaserLFO * phaserMix);
                }

                // Layer underwater sound effect (underwater preset only)
                if (underwaterMix > 0.0f && underwaterSoundSample.getNumSamples() > 0)
                {
                    // Calculate signal level from input signal (average of left and right)
                    float inputLevel = (std::abs(leftInput) + std::abs(rightInput)) * 0.5f;

                    // Update underwater envelope with attack/release
                    if (inputLevel > underwaterEnvelope)
                        underwaterEnvelope = inputLevel + envelopeAttack * (underwaterEnvelope - inputLevel);
                    else
                        underwaterEnvelope = inputLevel + envelopeRelease * (underwaterEnvelope - inputLevel);

                    // Read from underwater sound buffer (loop if necessary)
                    int underwaterChannel = underwaterSoundSample.getNumChannels() > 0 ? 0 : 0;
                    float underwaterSample = underwaterSoundSample.getSample(underwaterChannel, underwaterSoundReadPos);

                    // Apply envelope to underwater sound
                    underwaterSample *= underwaterEnvelope;

                    // Layer underwater sound with the input (not just delayed signal)
                    // This creates the underwater ambience effect
                    leftInput += underwaterSample * underwaterMix;
                    rightInput += underwaterSample * underwaterMix;

                    // Increment and wrap read position
                    underwaterSoundReadPos++;
                    if (underwaterSoundReadPos >= underwaterSoundSample.getNumSamples())
                        underwaterSoundReadPos = 0;
                }

                // Layer mechanical noise (tape preset only)
                if (mechanicalNoise > 0.0f && mechanicalNoiseSample.getNumSamples() > 0)
                {
                    float mechanicalAmount = mechanicalNoise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update mechanical envelope with attack/release
                    if (delayedLevel > mechanicalEnvelope)
                        mechanicalEnvelope = delayedLevel + envelopeAttack * (mechanicalEnvelope - delayedLevel);
                    else
                        mechanicalEnvelope = delayedLevel + envelopeRelease * (mechanicalEnvelope - delayedLevel);

                    // Read from mechanical noise buffer (loop if necessary)
                    int mechanicalChannel = mechanicalNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float mechanicalSample = mechanicalNoiseSample.getSample(mechanicalChannel, mechanicalNoiseReadPos);

                    // Apply mechanical noise amount and envelope
                    mechanicalSample *= mechanicalAmount * mechanicalEnvelope;

                    leftDelayed += mechanicalSample;
                    rightDelayed += mechanicalSample;

                    // Increment and wrap read position
                    mechanicalNoiseReadPos++;
                    if (mechanicalNoiseReadPos >= mechanicalNoiseSample.getNumSamples())
                        mechanicalNoiseReadPos = 0;
                }

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

    // Delay Time - can be note divisions (0-4) or milliseconds (0-15000) depending on mode
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_time", "Time",
        juce::NormalisableRange<float>(0.0f, 15000.0f, 1.0f, 0.3f),
        9000.0f)); // Default to 1/2 note (9000ms)

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

    // Pendulum Panning (auto-pan synced to BPM)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "pendulum_pan", "Pendulum Pan", false));

    // Noise/Static (telephone preset only) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "noise", "Noise", 0.0f, 100.0f, 0.0f));

    // Phaser Mix (telephone preset only) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "phaser_mix", "Phaser Mix", 0.0f, 100.0f, 0.0f));

    // Phaser Speed/Frequency (telephone preset only) - 0.1 to 10 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "phaser_speed", "Phaser Speed",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.1f, 0.3f),
        1.0f));

    // Underwater Mix (underwater preset only) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "underwater_mix", "Underwater Mix", 0.0f, 100.0f, 0.0f));

    // Mechanical Noise (tape preset only) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mechanical_noise", "Mechanical Noise", 0.0f, 100.0f, 0.0f));

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
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode (index 0)
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.30f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.276f); // 290 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.173f); // 4000 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.0f); // 0%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.0f); // 0%
        break;

    case 1: // Underwater - Deep, muffled underwater sound with bubbles
        parameters.getParameter("mix")->setValueNotifyingHost(0.60f); // 60%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.65f); // High feedback for swirly effect
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.0f); // 20 Hz (minimum - keep bass)
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.05f); // ~1500 Hz (heavy high cut)
        parameters.getParameter("wow")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("underwater_mix")->setValueNotifyingHost(0.50f); // 50% underwater sound
        break;

    case 2: // Tape - Classic cassette tape feel with wow and flutter
        parameters.getParameter("mix")->setValueNotifyingHost(0.50f); // 50%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.33f); // Time mode (index 1)
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.50f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(0.0f); // Off for vintage feel
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(55.0f); // 55 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(4700.0f); // 4700 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.16f); // 16% (tape warble)
        parameters.getParameter("flutter")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("mechanical_noise")->setValueNotifyingHost(0.0f); // 0% (off by default)
        break;

    case 3: // Radio - Vintage radio broadcast sound
        parameters.getParameter("mix")->setValueNotifyingHost(0.35f); // 35%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
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

//==============================================================================
// Load telephone noise sample from embedded binary data
void ReverbDelayPluginAudioProcessor::loadTelephoneNoise()
{
    // Load from embedded binary data
    // Note: BinaryData is generated by Projucer when you add files as binary resources

    // Get the embedded audio data directly from BinaryData
    const char* sourceData = BinaryData::telephone_noise_wav;
    int dataSize = BinaryData::telephone_noise_wavSize;

    if (sourceData == nullptr || dataSize == 0)
    {
        DBG("Telephone noise binary data not found - make sure to add telephone noise.wav to Projucer binary resources");
        return;
    }

    // Create audio format manager and register basic formats
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Create a memory input stream from the binary data
    std::unique_ptr<juce::MemoryInputStream> inputStream(new juce::MemoryInputStream(sourceData, dataSize, false));

    // Create reader for the WAV file from memory
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));

    if (reader != nullptr)
    {
        // Allocate buffer with same number of channels and length as the file
        telephoneNoiseSample.setSize(static_cast<int>(reader->numChannels),
                                      static_cast<int>(reader->lengthInSamples));

        // Read the entire file into the buffer
        reader->read(&telephoneNoiseSample,
                     0,
                     static_cast<int>(reader->lengthInSamples),
                     0,
                     true,
                     true);

        DBG("Telephone noise loaded successfully from binary data: " + juce::String(reader->lengthInSamples) + " samples");
    }
    else
    {
        DBG("Failed to create reader for telephone noise binary data");
    }
}

//==============================================================================
// Load underwater sound effect sample
void ReverbDelayPluginAudioProcessor::loadUnderwaterSound()
{
    // Load underwater sound from embedded binary data
    const char* sourceData = BinaryData::underwater_sound_effect_wav;
    int dataSize = BinaryData::underwater_sound_effect_wavSize;

    if (sourceData == nullptr || dataSize == 0)
    {
        DBG("Underwater sound binary data not found - make sure to add underwater sound effect.wav to Projucer binary resources");
        return;
    }

    // Create audio format manager and register basic formats
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Create a memory input stream from the binary data
    std::unique_ptr<juce::MemoryInputStream> inputStream(new juce::MemoryInputStream(sourceData, dataSize, false));

    // Create reader for the WAV file from memory
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));

    if (reader != nullptr)
    {
        // Allocate buffer with same number of channels and length as the file
        underwaterSoundSample.setSize(static_cast<int>(reader->numChannels),
                                      static_cast<int>(reader->lengthInSamples));

        // Read the entire file into the buffer
        reader->read(&underwaterSoundSample,
                     0,
                     static_cast<int>(reader->lengthInSamples),
                     0,
                     true,
                     true);

        DBG("Underwater sound loaded successfully from binary data: " + juce::String(reader->lengthInSamples) + " samples");
    }
    else
    {
        DBG("Failed to create reader for underwater sound binary data");
    }
}

//==============================================================================
// Load mechanical noise sample from embedded binary data
void ReverbDelayPluginAudioProcessor::loadMechanicalNoise()
{
    // Load mechanical noise from embedded binary data
    const char* sourceData = BinaryData::mechanical_noise_wav;
    int dataSize = BinaryData::mechanical_noise_wavSize;

    if (sourceData == nullptr || dataSize == 0)
    {
        DBG("Mechanical noise binary data not found - make sure to add mechanical noise.wav to Projucer binary resources");
        return;
    }

    // Create audio format manager and register basic formats
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Create a memory input stream from the binary data
    std::unique_ptr<juce::MemoryInputStream> inputStream(new juce::MemoryInputStream(sourceData, dataSize, false));

    // Create reader for the WAV file from memory
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));

    if (reader != nullptr)
    {
        // Allocate buffer with same number of channels and length as the file
        mechanicalNoiseSample.setSize(static_cast<int>(reader->numChannels),
                                      static_cast<int>(reader->lengthInSamples));

        // Read the entire file into the buffer
        reader->read(&mechanicalNoiseSample,
                     0,
                     static_cast<int>(reader->lengthInSamples),
                     0,
                     true,
                     true);

        DBG("Mechanical noise loaded successfully from binary data: " + juce::String(reader->lengthInSamples) + " samples");
    }
    else
    {
        DBG("Failed to create reader for mechanical noise binary data");
    }
}