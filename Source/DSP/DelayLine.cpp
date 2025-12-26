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
}

void DelayLine::reset()
{
    // Clear all samples in buffer
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;
}

float DelayLine::processSample(float input, float delayTimeMs, float feedback)
{
    // Calculate read position based on delay time
    float delaySamples = (delayTimeMs / 1000.0f) * static_cast<float>(sampleRate);

    // Calculate read position (wrap around buffer)
    int readPos = writePosition - static_cast<int>(delaySamples);
    if (readPos < 0)
        readPos += bufferSize;

    // Read delayed sample
    float delayedSample = buffer[readPos];

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
