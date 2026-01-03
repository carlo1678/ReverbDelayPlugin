#include "DelayLine.h"

DelayLine::DelayLine()
{
}

DelayLine::~DelayLine()
{
}

void DelayLine::prepare(double sr, int maxDelayTimeMs)
{
    sampleRate = sr;

    // Calculate buffer size (max delay time + safety margin)
    bufferSize = static_cast<int>((maxDelayTimeMs / 1000.0) * sampleRate) + 1;

    // Resize and clear buffer
    buffer.resize(bufferSize, 0.0f);
    writePosition = 0;
    reversePhase = 0;

    // Prepare pitch shifter
    pitchShifter.prepare(sr, bufferSize);
}

void DelayLine::reset()
{
    // Clear all samples in buffer
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;
    reversePhase = 0;

    // Reset pitch shifter
    pitchShifter.reset();
}

float DelayLine::processSample(float input, float delayTimeMs, float feedback, float pitchShift, bool reverse)
{
    // Calculate delay in samples
    float delaySamples = (delayTimeMs / 1000.0f) * static_cast<float>(sampleRate);
    int delaySamplesInt = static_cast<int>(delaySamples);

    // Ensure we have a valid delay time
    if (delaySamplesInt <= 0)
        delaySamplesInt = 1;
    if (delaySamplesInt >= bufferSize)
        delaySamplesInt = bufferSize - 1;

    // Read delayed sample
    float delayedSample = 0.0f;

    if (reverse)
    {
        // REVERSE DELAY IMPLEMENTATION:
        // For a delay time of T, we want to:
        // 1. Read a segment from (writePos - 2T) to (writePos - T)
        // 2. Play it backwards so the peak (at writePos - T) arrives first
        // 3. This ensures the reversed audio is "on beat"

        // Calculate the base position (end of the reversed segment)
        int segmentEnd = writePosition - delaySamplesInt;
        if (segmentEnd < 0)
            segmentEnd += bufferSize;

        // Read backwards from the end using reversePhase
        // reversePhase goes from 0 to delaySamplesInt, moving us backwards through the segment
        int readPos = segmentEnd - reversePhase;
        if (readPos < 0)
            readPos += bufferSize;

        delayedSample = buffer[readPos];

        // Increment reverse phase (moves us backwards through the segment)
        reversePhase++;
        if (reversePhase >= delaySamplesInt)
            reversePhase = 0;  // Loop the reversed segment
    }
    else
    {
        // NORMAL DELAY:
        // Simply read from delaySamples ago
        int readPos = writePosition - delaySamplesInt;
        if (readPos < 0)
            readPos += bufferSize;

        delayedSample = buffer[readPos];

        // Reset reverse phase when not in reverse mode
        reversePhase = 0;
    }

    // Apply pitch shifting to delayed sample
    if (pitchShift != 0.0f)
        delayedSample = pitchShifter.processSample(delayedSample, pitchShift);

    // Write input + feedback to buffer
    buffer[writePosition] = input + (delayedSample * feedback);

    // Advance write position (wrap around)
    writePosition++;
    if (writePosition >= bufferSize)
        writePosition = 0;

    return delayedSample;
}

void DelayLine::setDelayTime(float delayMs)
{
    currentDelayTime = delayMs;
}

void DelayLine::setFeedback(float feedback)
{
    currentFeedback = juce::jlimit(0.0f, 0.95f, feedback);
}
