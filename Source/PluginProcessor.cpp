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
    reverseWetParam = parameters.getRawParameterValue("reverse_wet");
    tempoSyncParam = parameters.getRawParameterValue("tempo_sync");
    pingPongParam = parameters.getRawParameterValue("ping_pong");
    timeModeParam = parameters.getRawParameterValue("time_mode");
    lowCutParam = parameters.getRawParameterValue("low_cut");
    highCutParam = parameters.getRawParameterValue("high_cut");
    wowParam = parameters.getRawParameterValue("wow");
    flutterParam = parameters.getRawParameterValue("flutter");
    pendulumPanParam = parameters.getRawParameterValue("pendulum_pan");
    pendulumSpeedParam = parameters.getRawParameterValue("pendulum_speed");
    noiseParam = parameters.getRawParameterValue("noise");
    phaserMixParam = parameters.getRawParameterValue("phaser_mix");
    phaserSpeedParam = parameters.getRawParameterValue("phaser_speed");
    underwaterMixParam = parameters.getRawParameterValue("underwater_mix");
    mechanicalNoiseParam = parameters.getRawParameterValue("mechanical_noise");
    radioNoiseParam = parameters.getRawParameterValue("radio_noise");
    delayPitchParam = parameters.getRawParameterValue("delay_pitch");

    // Debug: Plugin constructor started
    DBG("========================================");
    DBG("REVERB DELAY PLUGIN CONSTRUCTOR STARTED");
    DBG("========================================");

    // Load audio samples
    loadTelephoneNoise();
    loadUnderwaterSound();
    loadMechanicalNoise();
    loadRadioNoise();

    // Debug: Report loaded sample status
    DBG("=== Audio Sample Loading Status ===");
    DBG("Telephone Noise: " + juce::String(telephoneNoiseSample.getNumSamples()) + " samples");
    DBG("Underwater Sound: " + juce::String(underwaterSoundSample.getNumSamples()) + " samples");
    DBG("Mechanical Noise: " + juce::String(mechanicalNoiseSample.getNumSamples()) + " samples");
    DBG("Radio Noise: " + juce::String(radioNoiseSample.getNumSamples()) + " samples");

    // Also write to log file as fallback
    juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile("ReverbDelayPlugin_Debug.txt");
    logFile.appendText("========================================\n");
    logFile.appendText("REVERB DELAY PLUGIN CONSTRUCTOR - " + juce::Time::getCurrentTime().toString(true, true) + "\n");
    logFile.appendText("========================================\n");
    logFile.appendText("Telephone Noise: " + juce::String(telephoneNoiseSample.getNumSamples()) + " samples\n");
    logFile.appendText("Underwater Sound: " + juce::String(underwaterSoundSample.getNumSamples()) + " samples\n");
    logFile.appendText("Mechanical Noise: " + juce::String(mechanicalNoiseSample.getNumSamples()) + " samples\n");
    logFile.appendText("Radio Noise: " + juce::String(radioNoiseSample.getNumSamples()) + " samples\n");
    logFile.appendText("\n");

    DBG("========================================");
    DBG("Log file written to: " + logFile.getFullPathName());
    DBG("========================================");
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

    // Prepare independent reverse effect buffers (1/2 note at 120 BPM default, max 4 seconds for safety)
    int maxReverseSamples = static_cast<int>(sampleRate * 4.0); // 4 seconds max
    reverseCaptureBufferLeft.resize(maxReverseSamples, 0.0f);
    reverseCaptureBufferRight.resize(maxReverseSamples, 0.0f);
    reversePlaybackBufferLeft.resize(maxReverseSamples, 0.0f);
    reversePlaybackBufferRight.resize(maxReverseSamples, 0.0f);
    reverseCapturePos = 0;
    reversePlaybackPos = 0;
    reverseChunkSize = 0;
    reverseLockedChunkSize = 0;
    reverseIsCapturing = true;
    reverseBufferReady = false;

    // Set crossfade length to 15ms to avoid clicks/pops
    reverseCrossfadeLength = static_cast<int>(sampleRate * 0.015); // 15ms crossfade

    // Try to get initial BPM from host on startup
    // This ensures tempo-synced delays work correctly from the first note
    auto playHead = getPlayHead();
    if (playHead != nullptr)
    {
        juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
        if (posInfo.hasValue() && posInfo->getBpm().hasValue())
        {
            lastKnownBpm = *posInfo->getBpm();
        }
    }

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
    float radioNoise = radioNoiseParam->load(); // 0-100 range

    // Get delay pitch parameter and convert index to semitones
    // Index 0-8 maps to: -24, -12, -7, -5, 0, +5, +7, +12, +24 semitones
    int pitchIndex = static_cast<int>(delayPitchParam->load());
    float pitchShift = 0.0f;
    switch (pitchIndex)
    {
        case 0: pitchShift = -24.0f; break;  // -2 octaves
        case 1: pitchShift = -12.0f; break;  // -1 octave
        case 2: pitchShift = -7.0f; break;   // -Perfect 5th
        case 3: pitchShift = -5.0f; break;   // -Perfect 4th
        case 4: pitchShift = 0.0f; break;    // No pitch shift (unison)
        case 5: pitchShift = 5.0f; break;    // +Perfect 4th
        case 6: pitchShift = 7.0f; break;    // +Perfect 5th
        case 7: pitchShift = 12.0f; break;   // +1 octave
        case 8: pitchShift = 24.0f; break;   // +2 octaves
        default: pitchShift = 0.0f; break;
    }

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

        double bpm = lastKnownBpm; // Use last known BPM (initialized in prepareToPlay)

        if (tempoSyncEnabled)
        {
            // Get tempo from host and update last known BPM
            auto playHead = getPlayHead();
            if (playHead != nullptr)
            {
                juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
                if (posInfo.hasValue() && posInfo->getBpm().hasValue())
                {
                    bpm = *posInfo->getBpm();
                    lastKnownBpm = bpm; // Store for next time
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
    float pendulumFreq = 0.0f;
    if (pendulumPanEnabled)
    {
        double bpm = lastKnownBpm; // Use last known BPM
        auto playHead = getPlayHead();
        if (playHead != nullptr)
        {
            juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
            if (posInfo.hasValue() && posInfo->getBpm().hasValue())
            {
                bpm = *posInfo->getBpm();
                lastKnownBpm = bpm; // Store for next time
            }
        }

        // Get pendulum speed setting (0=4 Bar, 1=2 Bar, 2=1 Bar, 3=1/2, 4=1/4, 5=1/8, 6=1/16)
        int speedIndex = static_cast<int>(pendulumSpeedParam->load());

        // Calculate beats per cycle based on speed selection
        float beatsPerCycle = 1.0f; // Default: 1/4 bar = 1 beat
        switch (speedIndex)
        {
            case 0: beatsPerCycle = 16.0f; break;  // 4 Bar = 16 beats
            case 1: beatsPerCycle = 8.0f; break;   // 2 Bar = 8 beats
            case 2: beatsPerCycle = 4.0f; break;   // 1 Bar = 4 beats
            case 3: beatsPerCycle = 2.0f; break;   // 1/2 Bar = 2 beats
            case 4: beatsPerCycle = 1.0f; break;   // 1/4 Bar = 1 beat
            case 5: beatsPerCycle = 0.5f; break;   // 1/8 Bar = 0.5 beats
            case 6: beatsPerCycle = 0.25f; break;  // 1/16 Bar = 0.25 beats
            default: beatsPerCycle = 1.0f; break;
        }

        // Calculate frequency: Hz = (BPM / 60) / beatsPerCycle
        double beatsPerSecond = bpm / 60.0;
        pendulumFreq = static_cast<float>(beatsPerSecond / beatsPerCycle);
    }

    // Calculate desired reverse chunk size (always 1/2 note, tempo-synced)
    // Don't update locked size mid-operation - only when starting new capture
    int desiredReverseChunkSize = 0;
    if (reverseEnabled)
    {
        double bpm = lastKnownBpm;
        auto playHead = getPlayHead();
        if (playHead != nullptr)
        {
            juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo = playHead->getPosition();
            if (posInfo.hasValue() && posInfo->getBpm().hasValue())
            {
                bpm = *posInfo->getBpm();
                lastKnownBpm = bpm;
            }
        }

        // Calculate samples for 1/2 note: (60 / BPM) * sampleRate * 2 beats
        double secondsPerBeat = 60.0 / bpm;
        double secondsPerHalfNote = secondsPerBeat * 2.0; // 2 beats = 1/2 note in 4/4 time
        desiredReverseChunkSize = static_cast<int>(secondsPerHalfNote * lastSampleRate);

        // Clamp to buffer size
        int maxSize = static_cast<int>(reverseCaptureBufferLeft.size());
        if (desiredReverseChunkSize > maxSize)
            desiredReverseChunkSize = maxSize;
        if (desiredReverseChunkSize < reverseCrossfadeLength * 2)
            desiredReverseChunkSize = reverseCrossfadeLength * 2; // Minimum size for crossfading
    }
    else
    {
        // Reset reverse state when disabled
        reverseIsCapturing = true;
        reverseCapturePos = 0;
        reversePlaybackPos = 0;
        reverseBufferReady = false;
        reverseLockedChunkSize = 0;
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
                float leftDelayed = delayLineLeft.processSample(rightInput, delayTime, feedback, pitchShift, wow, flutter);
                float rightDelayed = delayLineRight.processSample(leftInput, delayTime, feedback, pitchShift, wow, flutter);

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

                // Apply pendulum panning if enabled (update phase)
                if (pendulumPanEnabled)
                {
                    // Update pendulum phase
                    pendulumPhase += (2.0f * juce::MathConstants<float>::pi * pendulumFreq) / static_cast<float>(lastSampleRate);
                    if (pendulumPhase >= 2.0f * juce::MathConstants<float>::pi)
                        pendulumPhase -= 2.0f * juce::MathConstants<float>::pi;
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

                    // Apply noise amount scaling and envelope with +6dB boost for audibility
                    noiseSample *= noiseAmount * telephoneEnvelope * 2.0f;

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
                // Bubbles volume modulated by DELAYED signal envelope for natural breathing effect
                if (underwaterMix > 0.0f && underwaterSoundSample.getNumSamples() > 0)
                {
                    // Calculate signal level from DELAYED signal (average of left and right)
                    // This makes bubbles fade with the delay tail
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Envelope follower with attack/release for smooth breathing
                    // Attack: 5ms (fast response), Release: 150ms (smooth fade)
                    float attackCoeff = 0.95f;  // Fast attack
                    float releaseCoeff = 0.998f; // Slow release for natural decay

                    if (delayedLevel > underwaterEnvelope)
                        underwaterEnvelope = delayedLevel * (1.0f - attackCoeff) + underwaterEnvelope * attackCoeff;
                    else
                        underwaterEnvelope = delayedLevel * (1.0f - releaseCoeff) + underwaterEnvelope * releaseCoeff;

                    // Read from underwater sound buffer (loop continuously)
                    int underwaterChannel = underwaterSoundSample.getNumChannels() > 0 ? 0 : 0;
                    float underwaterSample = underwaterSoundSample.getSample(underwaterChannel, underwaterSoundReadPos);

                    // Apply envelope follower to bubble audio
                    // Gain multiplier = envelope * underwater_mix * max_bubble_level
                    float maxBubbleLevel = 0.6f; // Cap bubbles at 60% to prevent overpowering delay
                    float bubbleGain = underwaterEnvelope * underwaterMix * maxBubbleLevel;
                    underwaterSample *= bubbleGain;

                    // Layer bubbles onto delayed signal (breathing with the delay tail)
                    leftDelayed += underwaterSample;
                    rightDelayed += underwaterSample;

                    // Increment and wrap read position
                    underwaterSoundReadPos++;
                    if (underwaterSoundReadPos >= underwaterSoundSample.getNumSamples())
                        underwaterSoundReadPos = 0;
                }

                // Layer mechanical noise (tape preset only)
                if (mechanicalNoise > 0.0f && mechanicalNoiseSample.getNumSamples() > 0)
                {
                    // Debug: Log first time mechanical noise is triggered
                    if (!mechanicalDebugLogged)
                    {
                        DBG(">>> MECHANICAL NOISE ACTIVE: Amount=" + juce::String(mechanicalNoise) + "%, Samples=" + juce::String(mechanicalNoiseSample.getNumSamples()));
                        mechanicalDebugLogged = true;
                    }

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

                // Layer radio noise (radio preset only)
                if (radioNoise > 0.0f && radioNoiseSample.getNumSamples() > 0)
                {
                    float radioAmount = radioNoise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update radio envelope with attack/release
                    if (delayedLevel > radioEnvelope)
                        radioEnvelope = delayedLevel + envelopeAttack * (radioEnvelope - delayedLevel);
                    else
                        radioEnvelope = delayedLevel + envelopeRelease * (radioEnvelope - delayedLevel);

                    // Read from radio noise buffer (loop if necessary)
                    int radioChannel = radioNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float radioSample = radioNoiseSample.getSample(radioChannel, radioNoiseReadPos);

                    // Apply radio noise amount and envelope
                    radioSample *= radioAmount * radioEnvelope;

                    leftDelayed += radioSample;
                    rightDelayed += radioSample;

                    // Increment and wrap read position
                    radioNoiseReadPos++;
                    if (radioNoiseReadPos >= radioNoiseSample.getNumSamples())
                        radioNoiseReadPos = 0;
                }

                // Process independent reverse effect (tempo-synced at 1/2 note)
                float leftReversed = 0.0f;
                float rightReversed = 0.0f;

                if (reverseEnabled)
                {
                    // Lock chunk size at start of new capture to prevent mid-operation changes
                    if (reverseIsCapturing && reverseCapturePos == 0)
                    {
                        reverseLockedChunkSize = desiredReverseChunkSize;
                    }

                    if (reverseIsCapturing)
                    {
                        // Capture incoming dry audio
                        if (reverseCapturePos < reverseLockedChunkSize)
                        {
                            reverseCaptureBufferLeft[reverseCapturePos] = leftInput;
                            reverseCaptureBufferRight[reverseCapturePos] = rightInput;
                            reverseCapturePos++;
                        }

                        // When capture buffer is full, reverse it
                        if (reverseCapturePos >= reverseLockedChunkSize)
                        {
                            // Reverse the captured audio into playback buffers
                            for (int i = 0; i < reverseLockedChunkSize; ++i)
                            {
                                reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                                reversePlaybackBufferRight[i] = reverseCaptureBufferRight[reverseLockedChunkSize - 1 - i];
                            }

                            // Switch to playback mode
                            reverseIsCapturing = false;
                            reversePlaybackPos = 0;
                            reverseCapturePos = 0;
                            reverseBufferReady = true;
                        }

                        // Output reversed audio if available (with crossfade)
                        if (reverseBufferReady && reversePlaybackPos < reverseLockedChunkSize)
                        {
                            leftReversed = reversePlaybackBufferLeft[reversePlaybackPos];
                            rightReversed = reversePlaybackBufferRight[reversePlaybackPos];
                            reversePlaybackPos++;
                        }
                    }
                    else
                    {
                        // Playback reversed audio while capturing new audio
                        if (reversePlaybackPos < reverseLockedChunkSize)
                        {
                            leftReversed = reversePlaybackBufferLeft[reversePlaybackPos];
                            rightReversed = reversePlaybackBufferRight[reversePlaybackPos];

                            // Apply crossfade at the end of playback to smooth transition
                            int crossfadeStart = reverseLockedChunkSize - reverseCrossfadeLength;
                            if (reversePlaybackPos >= crossfadeStart)
                            {
                                float crossfadeGain = 1.0f - (static_cast<float>(reversePlaybackPos - crossfadeStart) / static_cast<float>(reverseCrossfadeLength));
                                leftReversed *= crossfadeGain;
                                rightReversed *= crossfadeGain;
                            }

                            reversePlaybackPos++;
                        }

                        // Simultaneously capture new audio
                        if (reverseCapturePos < reverseLockedChunkSize)
                        {
                            reverseCaptureBufferLeft[reverseCapturePos] = leftInput;
                            reverseCaptureBufferRight[reverseCapturePos] = rightInput;
                            reverseCapturePos++;
                        }

                        // When both buffers complete, reverse and restart
                        if (reversePlaybackPos >= reverseLockedChunkSize && reverseCapturePos >= reverseLockedChunkSize)
                        {
                            // Reverse the newly captured audio
                            for (int i = 0; i < reverseLockedChunkSize; ++i)
                            {
                                reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                                reversePlaybackBufferRight[i] = reverseCaptureBufferRight[reverseLockedChunkSize - 1 - i];
                            }

                            // Restart both playback and capture
                            reversePlaybackPos = 0;
                            reverseCapturePos = 0;

                            // Update locked chunk size for next cycle with new desired size
                            reverseLockedChunkSize = desiredReverseChunkSize;
                        }
                    }
                }

                // Mix dry and wet (swapped for ping pong)
                float finalLeft = leftInput * (1.0f - mix) + leftDelayed * mix;
                float finalRight = rightInput * (1.0f - mix) + rightDelayed * mix;

                // Layer reversed audio on top (independent from delay)
                if (reverseEnabled)
                {
                    float reverseWet = reverseWetParam->load() / 100.0f; // Convert 0-100 to 0-1
                    finalLeft += leftReversed * reverseWet;
                    finalRight += rightReversed * reverseWet;
                }

                // Apply pendulum panning to final mixed output
                if (pendulumPanEnabled)
                {
                    // Calculate pan position using sine wave, limited to 75% width (-0.75 to +0.75)
                    float panPosition = std::sin(pendulumPhase) * 0.75f;

                    // Convert pan position to stereo gains (constant power panning)
                    // panPosition: -0.75 = 75% left, 0 = center, +0.75 = 75% right
                    float angle = (panPosition + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // Map to 0 to pi/2
                    float leftGain = std::cos(angle);
                    float rightGain = std::sin(angle);

                    // Apply panning to final output
                    leftChannel[sample] = finalLeft * leftGain;
                    rightChannel[sample] = finalRight * rightGain;
                }
                else
                {
                    leftChannel[sample] = finalLeft;
                    rightChannel[sample] = finalRight;
                }
            }
        }
        else
        {
            // Normal mode: delays stay on same channel
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float leftInput = leftChannel[sample];
                float rightInput = rightChannel[sample];

                float leftDelayed = delayLineLeft.processSample(leftInput, delayTime, feedback, pitchShift, wow, flutter);
                float rightDelayed = delayLineRight.processSample(rightInput, delayTime, feedback, pitchShift, wow, flutter);

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

                    // Apply noise amount scaling and envelope with +6dB boost for audibility
                    noiseSample *= noiseAmount * telephoneEnvelope * 2.0f;

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
                // Bubbles volume modulated by DELAYED signal envelope for natural breathing effect
                if (underwaterMix > 0.0f && underwaterSoundSample.getNumSamples() > 0)
                {
                    // Calculate signal level from DELAYED signal (average of left and right)
                    // This makes bubbles fade with the delay tail
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Envelope follower with attack/release for smooth breathing
                    // Attack: 5ms (fast response), Release: 150ms (smooth fade)
                    float attackCoeff = 0.95f;  // Fast attack
                    float releaseCoeff = 0.998f; // Slow release for natural decay

                    if (delayedLevel > underwaterEnvelope)
                        underwaterEnvelope = delayedLevel * (1.0f - attackCoeff) + underwaterEnvelope * attackCoeff;
                    else
                        underwaterEnvelope = delayedLevel * (1.0f - releaseCoeff) + underwaterEnvelope * releaseCoeff;

                    // Read from underwater sound buffer (loop continuously)
                    int underwaterChannel = underwaterSoundSample.getNumChannels() > 0 ? 0 : 0;
                    float underwaterSample = underwaterSoundSample.getSample(underwaterChannel, underwaterSoundReadPos);

                    // Apply envelope follower to bubble audio
                    // Gain multiplier = envelope * underwater_mix * max_bubble_level
                    float maxBubbleLevel = 0.6f; // Cap bubbles at 60% to prevent overpowering delay
                    float bubbleGain = underwaterEnvelope * underwaterMix * maxBubbleLevel;
                    underwaterSample *= bubbleGain;

                    // Layer bubbles onto delayed signal (breathing with the delay tail)
                    leftDelayed += underwaterSample;
                    rightDelayed += underwaterSample;

                    // Increment and wrap read position
                    underwaterSoundReadPos++;
                    if (underwaterSoundReadPos >= underwaterSoundSample.getNumSamples())
                        underwaterSoundReadPos = 0;
                }

                // Layer mechanical noise (tape preset only)
                if (mechanicalNoise > 0.0f && mechanicalNoiseSample.getNumSamples() > 0)
                {
                    // Debug: Log first time mechanical noise is triggered
                    if (!mechanicalDebugLogged)
                    {
                        DBG(">>> MECHANICAL NOISE ACTIVE: Amount=" + juce::String(mechanicalNoise) + "%, Samples=" + juce::String(mechanicalNoiseSample.getNumSamples()));
                        mechanicalDebugLogged = true;
                    }

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

                // Layer radio noise (radio preset only)
                if (radioNoise > 0.0f && radioNoiseSample.getNumSamples() > 0)
                {
                    float radioAmount = radioNoise / 100.0f; // 0-1 range

                    // Calculate signal level from delayed signal (average of left and right)
                    float delayedLevel = (std::abs(leftDelayed) + std::abs(rightDelayed)) * 0.5f;

                    // Update radio envelope with attack/release
                    if (delayedLevel > radioEnvelope)
                        radioEnvelope = delayedLevel + envelopeAttack * (radioEnvelope - delayedLevel);
                    else
                        radioEnvelope = delayedLevel + envelopeRelease * (radioEnvelope - delayedLevel);

                    // Read from radio noise buffer (loop if necessary)
                    int radioChannel = radioNoiseSample.getNumChannels() > 0 ? 0 : 0;
                    float radioSample = radioNoiseSample.getSample(radioChannel, radioNoiseReadPos);

                    // Apply radio noise amount and envelope
                    radioSample *= radioAmount * radioEnvelope;

                    leftDelayed += radioSample;
                    rightDelayed += radioSample;

                    // Increment and wrap read position
                    radioNoiseReadPos++;
                    if (radioNoiseReadPos >= radioNoiseSample.getNumSamples())
                        radioNoiseReadPos = 0;
                }

                // Process independent reverse effect (tempo-synced at 1/2 note)
                float leftReversed = 0.0f;
                float rightReversed = 0.0f;

                if (reverseEnabled)
                {
                    // Lock chunk size at start of new capture to prevent mid-operation changes
                    if (reverseIsCapturing && reverseCapturePos == 0)
                    {
                        reverseLockedChunkSize = desiredReverseChunkSize;
                    }

                    if (reverseIsCapturing)
                    {
                        // Capture incoming dry audio
                        if (reverseCapturePos < reverseLockedChunkSize)
                        {
                            reverseCaptureBufferLeft[reverseCapturePos] = leftInput;
                            reverseCaptureBufferRight[reverseCapturePos] = rightInput;
                            reverseCapturePos++;
                        }

                        // When capture buffer is full, reverse it
                        if (reverseCapturePos >= reverseLockedChunkSize)
                        {
                            // Reverse the captured audio into playback buffers
                            for (int i = 0; i < reverseLockedChunkSize; ++i)
                            {
                                reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                                reversePlaybackBufferRight[i] = reverseCaptureBufferRight[reverseLockedChunkSize - 1 - i];
                            }

                            // Switch to playback mode
                            reverseIsCapturing = false;
                            reversePlaybackPos = 0;
                            reverseCapturePos = 0;
                            reverseBufferReady = true;
                        }

                        // Output reversed audio if available (with crossfade)
                        if (reverseBufferReady && reversePlaybackPos < reverseLockedChunkSize)
                        {
                            leftReversed = reversePlaybackBufferLeft[reversePlaybackPos];
                            rightReversed = reversePlaybackBufferRight[reversePlaybackPos];
                            reversePlaybackPos++;
                        }
                    }
                    else
                    {
                        // Playback reversed audio while capturing new audio
                        if (reversePlaybackPos < reverseLockedChunkSize)
                        {
                            leftReversed = reversePlaybackBufferLeft[reversePlaybackPos];
                            rightReversed = reversePlaybackBufferRight[reversePlaybackPos];

                            // Apply crossfade at the end of playback to smooth transition
                            int crossfadeStart = reverseLockedChunkSize - reverseCrossfadeLength;
                            if (reversePlaybackPos >= crossfadeStart)
                            {
                                float crossfadeGain = 1.0f - (static_cast<float>(reversePlaybackPos - crossfadeStart) / static_cast<float>(reverseCrossfadeLength));
                                leftReversed *= crossfadeGain;
                                rightReversed *= crossfadeGain;
                            }

                            reversePlaybackPos++;
                        }

                        // Simultaneously capture new audio
                        if (reverseCapturePos < reverseLockedChunkSize)
                        {
                            reverseCaptureBufferLeft[reverseCapturePos] = leftInput;
                            reverseCaptureBufferRight[reverseCapturePos] = rightInput;
                            reverseCapturePos++;
                        }

                        // When both buffers complete, reverse and restart
                        if (reversePlaybackPos >= reverseLockedChunkSize && reverseCapturePos >= reverseLockedChunkSize)
                        {
                            // Reverse the newly captured audio
                            for (int i = 0; i < reverseLockedChunkSize; ++i)
                            {
                                reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                                reversePlaybackBufferRight[i] = reverseCaptureBufferRight[reverseLockedChunkSize - 1 - i];
                            }

                            // Restart both playback and capture
                            reversePlaybackPos = 0;
                            reverseCapturePos = 0;

                            // Update locked chunk size for next cycle with new desired size
                            reverseLockedChunkSize = desiredReverseChunkSize;
                        }
                    }
                }

                // Mix dry and wet signals
                float finalLeft = leftInput * (1.0f - mix) + leftDelayed * mix;
                float finalRight = rightInput * (1.0f - mix) + rightDelayed * mix;

                // Layer reversed audio on top (independent from delay)
                if (reverseEnabled)
                {
                    float reverseWet = reverseWetParam->load() / 100.0f; // Convert 0-100 to 0-1
                    finalLeft += leftReversed * reverseWet;
                    finalRight += rightReversed * reverseWet;
                }

                // Apply pendulum panning to final mixed output
                if (pendulumPanEnabled)
                {
                    // Calculate pan position using sine wave, limited to 75% width (-0.75 to +0.75)
                    float panPosition = std::sin(pendulumPhase) * 0.75f;

                    // Convert pan position to stereo gains (constant power panning)
                    // panPosition: -0.75 = 75% left, 0 = center, +0.75 = 75% right
                    float angle = (panPosition + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // Map to 0 to pi/2
                    float leftGain = std::cos(angle);
                    float rightGain = std::sin(angle);

                    // Apply panning to final output
                    leftChannel[sample] = finalLeft * leftGain;
                    rightChannel[sample] = finalRight * rightGain;
                }
                else
                {
                    leftChannel[sample] = finalLeft;
                    rightChannel[sample] = finalRight;
                }
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
            float delayed = delayLineLeft.processSample(input, delayTime, feedback, pitchShift, wow, flutter);

            // Apply filters to delayed signal
            delayed = lowCutFilterLeft.get<0>().processSample(delayed);
            delayed = lowCutFilterLeft.get<1>().processSample(delayed);
            delayed = lowCutFilterLeft.get<2>().processSample(delayed);
            delayed = highCutFilterLeft.get<0>().processSample(delayed);
            delayed = highCutFilterLeft.get<1>().processSample(delayed);
            delayed = highCutFilterLeft.get<2>().processSample(delayed);

            // Process independent reverse effect (tempo-synced at 1/2 note, mono)
            float reversed = 0.0f;

            if (reverseEnabled)
            {
                // Lock chunk size at start of new capture to prevent mid-operation changes
                if (reverseIsCapturing && reverseCapturePos == 0)
                {
                    reverseLockedChunkSize = desiredReverseChunkSize;
                }

                if (reverseIsCapturing)
                {
                    // Capture incoming dry audio
                    if (reverseCapturePos < reverseLockedChunkSize)
                    {
                        reverseCaptureBufferLeft[reverseCapturePos] = input;
                        reverseCapturePos++;
                    }

                    // When capture buffer is full, reverse it
                    if (reverseCapturePos >= reverseLockedChunkSize)
                    {
                        // Reverse the captured audio into playback buffer
                        for (int i = 0; i < reverseLockedChunkSize; ++i)
                        {
                            reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                        }

                        // Switch to playback mode
                        reverseIsCapturing = false;
                        reversePlaybackPos = 0;
                        reverseCapturePos = 0;
                        reverseBufferReady = true;
                    }

                    // Output reversed audio if available
                    if (reverseBufferReady && reversePlaybackPos < reverseLockedChunkSize)
                    {
                        reversed = reversePlaybackBufferLeft[reversePlaybackPos];
                        reversePlaybackPos++;
                    }
                }
                else
                {
                    // Playback reversed audio while capturing new audio
                    if (reversePlaybackPos < reverseLockedChunkSize)
                    {
                        reversed = reversePlaybackBufferLeft[reversePlaybackPos];

                        // Apply crossfade at the end of playback to smooth transition
                        int crossfadeStart = reverseLockedChunkSize - reverseCrossfadeLength;
                        if (reversePlaybackPos >= crossfadeStart)
                        {
                            float crossfadeGain = 1.0f - (static_cast<float>(reversePlaybackPos - crossfadeStart) / static_cast<float>(reverseCrossfadeLength));
                            reversed *= crossfadeGain;
                        }

                        reversePlaybackPos++;
                    }

                    // Simultaneously capture new audio
                    if (reverseCapturePos < reverseLockedChunkSize)
                    {
                        reverseCaptureBufferLeft[reverseCapturePos] = input;
                        reverseCapturePos++;
                    }

                    // When both buffers complete, reverse and restart
                    if (reversePlaybackPos >= reverseLockedChunkSize && reverseCapturePos >= reverseLockedChunkSize)
                    {
                        // Reverse the newly captured audio
                        for (int i = 0; i < reverseLockedChunkSize; ++i)
                        {
                            reversePlaybackBufferLeft[i] = reverseCaptureBufferLeft[reverseLockedChunkSize - 1 - i];
                        }

                        // Restart both playback and capture
                        reversePlaybackPos = 0;
                        reverseCapturePos = 0;

                        // Update locked chunk size for next cycle with new desired size
                        reverseLockedChunkSize = desiredReverseChunkSize;
                    }
                }
            }

            // Mix dry and delayed signal
            float final = input * (1.0f - mix) + delayed * mix;

            // Layer reversed audio on top (independent from delay)
            if (reverseEnabled)
            {
                float reverseWet = reverseWetParam->load() / 100.0f; // Convert 0-100 to 0-1
                final += reversed * reverseWet;
            }

            leftChannel[sample] = final;
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
    // Save all parameters using APVTS built-in state management
    auto state = parameters.copyState();

    // Add current preset index to the state
    state.setProperty("currentPreset", currentPresetIndex, nullptr);

    // Convert to XML and save to memory block
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ReverbDelayPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameters from saved state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            // Restore all parameters exactly as they were saved
            auto state = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(state);

            // Restore current preset index
            if (state.hasProperty("currentPreset"))
            {
                currentPresetIndex = state.getProperty("currentPreset");
            }
        }
    }
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


    // Reverse Effect (independent tempo-synced reverse, always at 1/2 note)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "reverse_delay", "Reverse", false));

    // Reverse Wet Mix (0-100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "reverse_wet", "Reverse Wet", 0.0f, 100.0f, 50.0f));

    // Ping Pong (stereo bouncing delay)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "ping_pong", "Ping Pong", false));

    // Low Cut (highpass filter) - 20 Hz to 1000 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "low_cut", "Low Cut",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.3f),
        20.0f));

    // High Cut (lowpass filter) - 100 Hz to 20000 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "high_cut", "High Cut",
        juce::NormalisableRange<float>(100.0f, 20000.0f, 1.0f, 0.3f),
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

    // Pendulum Speed (tempo-synced divisions: 0=4 Bar, 1=2 Bar, 2=1 Bar, 3=1/2, 4=1/4, 5=1/8, 6=1/16)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pendulum_speed", "Pendulum Speed",
        juce::NormalisableRange<float>(0.0f, 6.0f, 1.0f),
        4.0f)); // Default to 1/4 Bar (index 4)

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

    // Radio Noise (radio preset only) - 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "radio_noise", "Radio Noise", 0.0f, 100.0f, 0.0f));

    // Delay Pitch (available on all presets)
    // Musical intervals: -2 octaves, -1 octave, -P5, -P4, 0, +P4, +P5, +1 octave, +2 octaves
    // Semitones: -24, -12, -7, -5, 0, +5, +7, +12, +24
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "delay_pitch", "Delay Pitch",
        juce::NormalisableRange<float>(0.0f, 8.0f, 1.0f),
        4.0f)); // Default to 0 semitones (index 4)

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
    // Store current preset index
    currentPresetIndex = presetIndex;

    switch (presetIndex)
    {
    case 0: // Telephone - Classic telephone effect with narrow frequency range
        parameters.getParameter("mix")->setValueNotifyingHost(0.40f); // 40%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode (index 0)
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.30f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("reverse_wet")->setValueNotifyingHost(50.0f); // 50% reverse mix
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        // Normalized values for skewed ranges (low_cut: 20-1000 Hz, high_cut: 100-20000 Hz, skew 0.3)
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.037f); // 385 Hz (high pass - cuts low end)
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.686f); // 5550 Hz (low pass - cuts high end)
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
        parameters.getParameter("reverse_wet")->setValueNotifyingHost(50.0f); // 50% reverse mix
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.00035f); // 110 Hz (high pass - removes very low frequencies)
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.0000064f); // 650 Hz (low pass - dark, muffled underwater sound)
        parameters.getParameter("wow")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("underwater_mix")->setValueNotifyingHost(0.50f); // 50% underwater sound
        break;

    case 2: // Tape - Classic cassette tape feel with wow and flutter
        parameters.getParameter("mix")->setValueNotifyingHost(0.50f); // 50%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.50f); // Moderate feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On (for tempo sync with Notes mode)
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("reverse_wet")->setValueNotifyingHost(50.0f); // 50% reverse mix
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        // Normalized values for skewed ranges (low_cut: 20-1000 Hz, high_cut: 100-20000 Hz, skew 0.3)
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.00005f); // 55 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.643f); // 4700 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.16f); // 16% (tape warble)
        parameters.getParameter("flutter")->setValueNotifyingHost(0.05f); // 5%
        parameters.getParameter("mechanical_noise")->setValueNotifyingHost(0.30f); // 30% mechanical noise
        break;

    case 3: // Radio - Vintage radio broadcast sound
        parameters.getParameter("mix")->setValueNotifyingHost(0.35f); // 35%
        parameters.getParameter("delay_time")->setValueNotifyingHost(9000.0f / 15000.0f); // 1/2 note
        parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes mode
        parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.20f); // Low feedback
        parameters.getParameter("tempo_sync")->setValueNotifyingHost(1.0f); // On
        parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f); // Off
        parameters.getParameter("reverse_wet")->setValueNotifyingHost(50.0f); // 50% reverse mix
        parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f); // Off
        // Normalized values for skewed ranges (low_cut: 20-1000 Hz, high_cut: 100-20000 Hz, skew 0.3)
        parameters.getParameter("low_cut")->setValueNotifyingHost(0.0136f); // 290 Hz
        parameters.getParameter("high_cut")->setValueNotifyingHost(0.595f); // 3550 Hz
        parameters.getParameter("wow")->setValueNotifyingHost(0.0f); // 0%
        parameters.getParameter("flutter")->setValueNotifyingHost(0.0f); // 0%
        parameters.getParameter("radio_noise")->setValueNotifyingHost(0.0f); // 0% (off by default)
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
    const char* sourceData = BinaryData::Mechanical_Noise_wav;
    int dataSize = BinaryData::Mechanical_Noise_wavSize;

    if (sourceData == nullptr || dataSize == 0)
    {
        DBG("Mechanical noise binary data not found - make sure to add Mechanical_Noise.wav to Projucer binary resources");
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

//==============================================================================
// Load radio noise sample from embedded binary data
void ReverbDelayPluginAudioProcessor::loadRadioNoise()
{
    // Load radio noise from embedded binary data
    const char* sourceData = BinaryData::Radio_Noise_wav;
    int dataSize = BinaryData::Radio_Noise_wavSize;

    if (sourceData == nullptr || dataSize == 0)
    {
        DBG("Radio noise binary data not found - make sure to add Radio_Noise.wav to Projucer binary resources");
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
        radioNoiseSample.setSize(static_cast<int>(reader->numChannels),
                                 static_cast<int>(reader->lengthInSamples));

        // Read the entire file into the buffer
        reader->read(&radioNoiseSample,
                     0,
                     static_cast<int>(reader->lengthInSamples),
                     0,
                     true,
                     true);

        DBG("Radio noise loaded successfully from binary data: " + juce::String(reader->lengthInSamples) + " samples");
    }
    else
    {
        DBG("Failed to create reader for radio noise binary data");
    }
}