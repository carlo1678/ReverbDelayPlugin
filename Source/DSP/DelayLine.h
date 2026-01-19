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
    float processSample(float input, float delayTimeMs, float feedback, float pitchShift = 0.0f, float wow = 0.0f, float flutter = 0.0f);

    // Setters
    void setDelayTime(float delayMs);
    void setFeedback(float feedback);

private:
    std::vector<float> buffer;
    int bufferSize;
    int writePosition;
    double sampleRate;

    float currentDelayTime;
    float currentFeedback;

    PitchShifter pitchShifter;

    // Wow and Flutter LFO state
    float wowPhase;        // Phase for wow LFO (0 to 2π)
    float flutterPhase;    // Phase for flutter LFO (0 to 2π)
};
