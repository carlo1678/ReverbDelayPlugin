#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PitchShifter.h"

class DelayLine
{
public:
    DelayLine();
    ~DelayLine();

    // Setup
    void prepare(double sampleRate, int maxDelayTimeMs);
    void reset();

    // Processing
    float processSample(float input, float delayTimeMs, float feedback, float pitchShift = 0.0f, bool reverse = false, float wow = 0.0f, float flutter = 0.0f);

    // Setters
    void setDelayTime(float delayMs);
    void setFeedback(float feedback);

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePosition = 0;
    double sampleRate = 44100.0;

    float currentDelayTime = 500.0f;
    float currentFeedback = 0.3f;

    PitchShifter pitchShifter;

    // Reverse delay - continuous reverse playback
    float reverseReadPosition = 0.0f;  // Floating point read position for smooth reverse playback

    // Wow and Flutter LFO state
    float wowPhase = 0.0f;      // Phase for wow LFO (0 to 2π)
    float flutterPhase = 0.0f;  // Phase for flutter LFO (0 to 2π)
};
