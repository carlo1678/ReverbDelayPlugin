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

    // Reverse delay - chunk-based reverse effect
    std::vector<float> reverseCaptureBuffer;  // Buffer to capture audio for reversing
    std::vector<float> reversePlaybackBuffer; // Reversed audio ready for playback
    int reverseCapturePos = 0;                // Current position in capture buffer
    int reversePlaybackPos = 0;               // Current position in playback buffer
    int reverseChunkSize = 0;                 // Size of reverse chunks (based on delay time)
    bool reverseIsCapturing = true;           // True: capturing, False: playing back
    bool reverseBufferReady = false;          // True when reverse buffer has been filled once

    // Wow and Flutter LFO state
    float wowPhase = 0.0f;      // Phase for wow LFO (0 to 2π)
    float flutterPhase = 0.0f;  // Phase for flutter LFO (0 to 2π)
};
