#pragma once

#include <JuceHeader.h>

class ReverbEngine
{
public:
    ReverbEngine();
    ~ReverbEngine();

    // Setup
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Processing
    void process(juce::AudioBuffer<float>& buffer);

    // Parameters
    void setRoomSize(float size);
    void setDamping(float damping);
    void setWetLevel(float wet);
    void setDryLevel(float dry);
    void setWidth(float width);

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    double sampleRate = 44100.0;
};