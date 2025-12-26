#pragma once

#include <JuceHeader.h>
#include <vector>

class DelayLine
{
public:
    DelayLine();
    ~DelayLine();

    // Setup
    void prepare(double sampleRate, int maxDelayTimeMs);
    void reset();

    // Processing
    float processSample(float input, float delayTimeMs, float feedback);

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
};
