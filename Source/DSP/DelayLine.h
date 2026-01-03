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
    float processSample(float input, float delayTimeMs, float feedback, float pitchShift = 0.0f, bool reverse = false);

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
    std::vector<float> reverseBuffer;
};
