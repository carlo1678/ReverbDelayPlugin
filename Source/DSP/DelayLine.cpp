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

    // Resize and clear buffers
    buffer.resize(bufferSize, 0.0f);
    writePosition = 0;
    reverseReadPosition = 0.0f;

    // Prepare pitch shifter
    pitchShifter.prepare(sr, bufferSize);
}

void DelayLine::reset()
{
    // Clear all samples in buffers
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;
    reverseReadPosition = 0.0f;

    // Reset pitch shifter
    pitchShifter.reset();
}

float DelayLine::processSample(float input, float delayTimeMs, float feedback, float pitchShift, bool reverse, float wow, float flutter)
{
    // Apply wow and flutter modulation to delay time
    // Wow: slow modulation (around 1 Hz), Flutter: fast modulation (around 8 Hz)
    float wowFreq = 1.0f;      // 1 Hz for wow
    float flutterFreq = 8.0f;  // 8 Hz for flutter

    // Update LFO phases
    wowPhase += (2.0f * juce::MathConstants<float>::pi * wowFreq) / static_cast<float>(sampleRate);
    flutterPhase += (2.0f * juce::MathConstants<float>::pi * flutterFreq) / static_cast<float>(sampleRate);

    // Wrap phases
    if (wowPhase >= 2.0f * juce::MathConstants<float>::pi)
        wowPhase -= 2.0f * juce::MathConstants<float>::pi;
    if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi)
        flutterPhase -= 2.0f * juce::MathConstants<float>::pi;

    // Generate LFO signals (sine waves)
    float wowLFO = std::sin(wowPhase);
    float flutterLFO = std::sin(flutterPhase);

    // Apply modulation to delay time
    // Wow/Flutter as percentage (0-100) converted to depth in milliseconds
    float wowDepth = (wow / 100.0f) * 5.0f;        // Up to 5ms modulation for wow
    float flutterDepth = (flutter / 100.0f) * 2.0f; // Up to 2ms modulation for flutter

    float modulatedDelayTime = delayTimeMs + (wowLFO * wowDepth) + (flutterLFO * flutterDepth);

    // Calculate delay in samples
    float delaySamples = (modulatedDelayTime / 1000.0f) * static_cast<float>(sampleRate);
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
        // CONTINUOUS REVERSE DELAY:
        // Reads from the delay buffer moving backwards in time
        // This creates a continuous reverse effect with no gaps or latency

        // Initialize reverse read position if needed (first time or after switching modes)
        if (reverseReadPosition <= 0.0f)
        {
            // Start reading from delay time ago (same as normal delay)
            reverseReadPosition = static_cast<float>(writePosition) - delaySamples;
            if (reverseReadPosition < 0.0f)
                reverseReadPosition += bufferSize;
        }

        // Read from buffer at reverse read position with linear interpolation
        int readPosInt = static_cast<int>(reverseReadPosition);
        float readPosFrac = reverseReadPosition - readPosInt;

        // Get two samples for interpolation
        int readPos1 = readPosInt % bufferSize;
        int readPos2 = (readPosInt + 1) % bufferSize;
        if (readPos1 < 0) readPos1 += bufferSize;
        if (readPos2 < 0) readPos2 += bufferSize;

        // Linear interpolation for smooth playback
        float sample1 = buffer[readPos1];
        float sample2 = buffer[readPos2];
        delayedSample = sample1 + readPosFrac * (sample2 - sample1);

        // Move read position BACKWARDS through the buffer (reverse playback)
        // We move forward in time but read backwards through captured audio
        reverseReadPosition -= 1.0f;

        // Wrap around when we reach the beginning
        if (reverseReadPosition < 0.0f)
            reverseReadPosition += bufferSize;
    }
    else
    {
        // NORMAL DELAY:
        // Simply read from delaySamples ago
        int readPos = writePosition - delaySamplesInt;
        if (readPos < 0)
            readPos += bufferSize;

        delayedSample = buffer[readPos];

        // Reset reverse state when not in reverse mode
        reverseReadPosition = 0.0f;
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
