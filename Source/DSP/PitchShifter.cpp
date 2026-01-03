#include "PitchShifter.h"

PitchShifter::PitchShifter()
{
}

PitchShifter::~PitchShifter()
{
}

void PitchShifter::prepare(double sr, int maxBufferSize)
{
    sampleRate = sr;
    bufferSize = maxBufferSize;

    // Allocate buffer for pitch shifting
    buffer.resize(windowSize * 2, 0.0f);
    writePosition = 0;
    phase = 0.0f;
}

void PitchShifter::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;
    phase = 0.0f;
}

float PitchShifter::processSample(float input, float pitchShiftSemitones)
{
    // Write input to buffer
    buffer[writePosition] = input;

    // Calculate pitch shift ratio (semitones to frequency ratio)
    float pitchRatio = std::pow(2.0f, pitchShiftSemitones / 12.0f);

    // Simple pitch shifting using variable delay
    float readPosition = writePosition - (windowSize * 0.5f);

    // Add pitch-based offset
    readPosition -= (pitchRatio - 1.0f) * windowSize * 0.25f;

    // Wrap read position
    while (readPosition < 0)
        readPosition += buffer.size();
    while (readPosition >= buffer.size())
        readPosition -= buffer.size();

    // Linear interpolation for smooth reading
    int readIndex = static_cast<int>(readPosition);
    float frac = readPosition - readIndex;

    int nextIndex = (readIndex + 1) % buffer.size();

    float sample1 = buffer[readIndex];
    float sample2 = buffer[nextIndex];

    float output = sample1 + frac * (sample2 - sample1);

    // Apply simple windowing to reduce artifacts
    float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase / windowSize));
    output *= window;

    // Update phase
    phase += pitchRatio;
    if (phase >= windowSize)
        phase -= windowSize;

    // Advance write position
    writePosition = (writePosition + 1) % buffer.size();

    return output;
}

void PitchShifter::setPitchShift(float semitones)
{
    currentPitchShift = juce::jlimit(-12.0f, 12.0f, semitones);
}