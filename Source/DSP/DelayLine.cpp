#include "DelayLine.h"
#include <algorithm>  // For std::fill
#include <cmath>      // For std::sin

DelayLine::DelayLine()
    : bufferSize(0),
      writePosition(0),
      sampleRate(44100.0),
      currentDelayTime(500.0f),
      currentFeedback(0.3f),
      wowPhase(0.0f),
      flutterPhase(0.0f)
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

    // Prepare pitch shifter
    pitchShifter.prepare(sr, bufferSize);
}

void DelayLine::reset()
{
    // Clear all samples in buffers
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;

    // Reset pitch shifter
    pitchShifter.reset();
}

float DelayLine::processSample(float input, float delayTimeMs, float feedback, float pitchShift, float wow, float flutter)
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

    // Read delayed sample from normal delay buffer
    int readPos = writePosition - delaySamplesInt;
    if (readPos < 0)
        readPos += bufferSize;

    float delayedSample = buffer[readPos];

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
