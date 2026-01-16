#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
 * Granular Pitch Shifter
 *
 * Professional-quality pitch shifting using overlapping windowed grains.
 * This maintains time while changing pitch, suitable for delay effects.
 *
 * Algorithm: Two-stream granular synthesis with Hann windowing
 */
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
    // Circular delay buffer for granular processing
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePosition = 0;
    double sampleRate = 44100.0;

    float currentPitchShift = 0.0f;

    // Granular synthesis parameters
    static constexpr int grainSize = 2048;      // Grain size in samples (~46ms at 44.1kHz)
    static constexpr int overlapFactor = 4;      // Number of overlapping grains
    static constexpr int hopSize = grainSize / overlapFactor;  // Distance between grain starts

    // Two grain streams for crossfading
    struct GrainStream
    {
        float readPosition = 0.0f;
        int grainProgress = 0;          // Current position within grain
        float grainPhase = 0.0f;        // Phase for window function
    };

    GrainStream grains[2];
    int currentGrain = 0;

    // Pre-calculated Hann window for smooth grain crossfading
    std::vector<float> hannWindow;

    // Helper functions
    void calculateHannWindow();
    float getWindowedSample(float readPos, float windowPhase);
    float readBufferInterpolated(float position);
};
