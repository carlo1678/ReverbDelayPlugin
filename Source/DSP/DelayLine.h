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
    int bufferSize;
    int writePosition;
    double sampleRate;

    float currentDelayTime;
    float currentFeedback;

    PitchShifter pitchShifter;

    // Reverse delay - chunk-based reverse effect
    std::vector<float> reverseCaptureBuffer;  // Buffer to capture audio for reversing
    std::vector<float> reversePlaybackBuffer; // Reversed audio ready for playback
    int reverseCapturePos;                    // Current position in capture buffer
    int reversePlaybackPos;                   // Current position in playback buffer
    int reverseChunkSize;                     // Size of reverse chunks (based on delay time)
    bool reverseIsCapturing;                  // True: capturing, False: playing back
    bool reverseBufferReady;                  // True when reverse buffer has been filled once

    // Wow and Flutter LFO state
    float wowPhase;        // Phase for wow LFO (0 to 2π)
    float flutterPhase;    // Phase for flutter LFO (0 to 2π)
};
