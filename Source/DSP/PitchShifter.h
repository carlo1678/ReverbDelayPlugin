#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter();

    // Setup
    void prepare(double sampleRate, int maxBufferSize);
    void reset();

    // Processing
    float processSample(float input, float pitchShiftSemitones);

    // Parameters
    void setPitchShift(float semitones);

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePosition = 0;
    double sampleRate = 44100.0;

    float currentPitchShift = 0.0f;

    // Window parameters for pitch shifting
    static constexpr int windowSize = 2048;
    float phase = 0.0f;
};