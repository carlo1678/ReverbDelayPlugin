#include "ReverbEngine.h"

ReverbEngine::ReverbEngine()
{
    // Set default reverb parameters
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 0.33f;
    reverbParams.dryLevel = 0.4f;
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;

    reverb.setParameters(reverbParams);
}

ReverbEngine::~ReverbEngine()
{
}

void ReverbEngine::prepare(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    reverb.setSampleRate(sr);
    reverb.reset();
}

void ReverbEngine::reset()
{
    reverb.reset();
}

void ReverbEngine::process(juce::AudioBuffer<float>& buffer)
{
    // Process reverb (works on stereo or mono)
    if (buffer.getNumChannels() == 1)
    {
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
    }
    else if (buffer.getNumChannels() >= 2)
    {
        reverb.processStereo(buffer.getWritePointer(0),
            buffer.getWritePointer(1),
            buffer.getNumSamples());
    }
}

void ReverbEngine::setRoomSize(float size)
{
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, size);
    reverb.setParameters(reverbParams);
}

void ReverbEngine::setDamping(float damping)
{
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, damping);
    reverb.setParameters(reverbParams);
}

void ReverbEngine::setWetLevel(float wet)
{
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, wet);
    reverb.setParameters(reverbParams);
}

void ReverbEngine::setDryLevel(float dry)
{
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, dry);
    reverb.setParameters(reverbParams);
}

void ReverbEngine::setWidth(float width)
{
    reverbParams.width = juce::jlimit(0.0f, 1.0f, width);
    reverb.setParameters(reverbParams);
}