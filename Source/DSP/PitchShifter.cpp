#include "PitchShifter.h"
#include <algorithm>

PitchShifter::PitchShifter()
{
}

PitchShifter::~PitchShifter()
{
}

void PitchShifter::prepare(double sr, int maxBufferSize)
{
    sampleRate = sr;
    bufferSize = std::max(maxBufferSize, grainSize * 4); // Ensure enough buffer for grains

    // Allocate circular buffer for granular processing
    buffer.resize(bufferSize, 0.0f);
    writePosition = 0;

    // Pre-calculate Hann window for grain crossfading
    calculateHannWindow();

    // Initialize grain streams
    for (int i = 0; i < 2; ++i)
    {
        grains[i].readPosition = grainSize * i / 2.0f; // Offset grains for overlap
        grains[i].grainProgress = 0;
        grains[i].grainPhase = 0.0f;
    }

    currentGrain = 0;
}

void PitchShifter::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;

    // Reset grain streams
    for (int i = 0; i < 2; ++i)
    {
        grains[i].readPosition = grainSize * i / 2.0f;
        grains[i].grainProgress = 0;
        grains[i].grainPhase = 0.0f;
    }
}

void PitchShifter::calculateHannWindow()
{
    // Pre-calculate Hann window for smooth grain transitions
    hannWindow.resize(grainSize);
    for (int i = 0; i < grainSize; ++i)
    {
        // Hann window: 0.5 * (1 - cos(2π * i / N))
        float phase = (float)i / (float)(grainSize - 1);
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));
    }
}

float PitchShifter::readBufferInterpolated(float position)
{
    // Cubic interpolation for high-quality sample reading
    // This provides much better quality than linear interpolation

    // Wrap position to buffer bounds
    while (position < 0)
        position += bufferSize;
    while (position >= bufferSize)
        position -= bufferSize;

    int index = static_cast<int>(position);
    float frac = position - index;

    // Get 4 samples for cubic interpolation
    int i0 = (index - 1 + bufferSize) % bufferSize;
    int i1 = index;
    int i2 = (index + 1) % bufferSize;
    int i3 = (index + 2) % bufferSize;

    float y0 = buffer[i0];
    float y1 = buffer[i1];
    float y2 = buffer[i2];
    float y3 = buffer[i3];

    // Cubic interpolation (Catmull-Rom spline)
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float PitchShifter::getWindowedSample(float readPos, float windowPhase)
{
    // Read sample with interpolation
    float sample = readBufferInterpolated(readPos);

    // Apply Hann window for smooth grain transitions
    int windowIndex = static_cast<int>(windowPhase * (grainSize - 1));
    windowIndex = juce::jlimit(0, grainSize - 1, windowIndex);

    return sample * hannWindow[windowIndex];
}

float PitchShifter::processSample(float input, float pitchShiftSemitones)
{
    // Write input to circular buffer
    buffer[writePosition] = input;
    writePosition = (writePosition + 1) % bufferSize;

    // Calculate pitch ratio from semitones
    // Ratio > 1.0 = pitch up, Ratio < 1.0 = pitch down
    float pitchRatio = std::pow(2.0f, pitchShiftSemitones / 12.0f);

    // If no pitch shift, bypass granular processing for efficiency
    if (std::abs(pitchShiftSemitones) < 0.01f)
    {
        int readPos = (writePosition - grainSize + bufferSize) % bufferSize;
        return buffer[readPos];
    }

    // Process two overlapping grain streams
    float output = 0.0f;
    int grainsActive = 0;

    for (int g = 0; g < 2; ++g)
    {
        GrainStream& grain = grains[g];

        // Calculate read position in buffer
        // Grains read backwards from write position
        float bufferReadPos = writePosition - (grainSize * 2 - grain.readPosition);
        while (bufferReadPos < 0)
            bufferReadPos += bufferSize;

        // Get windowed sample from this grain
        float windowPhase = (float)grain.grainProgress / (float)grainSize;
        float grainSample = getWindowedSample(bufferReadPos, windowPhase);

        output += grainSample;
        grainsActive++;

        // Advance grain playback at pitch-adjusted rate
        grain.readPosition += pitchRatio;
        grain.grainProgress++;

        // When grain finishes, restart it
        if (grain.grainProgress >= grainSize)
        {
            grain.grainProgress = 0;
            grain.readPosition = 0.0f;
        }
    }

    // Normalize output by number of active grains to prevent volume increase
    if (grainsActive > 0)
        output /= grainsActive;

    return output;
}

void PitchShifter::setPitchShift(float semitones)
{
    currentPitchShift = juce::jlimit(-24.0f, 24.0f, semitones);
}
