#include "DelayLine.h"
#include <algorithm>  // For std::fill
#include <cmath>      // For std::sin

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

    // Prepare reverse delay buffers
    reverseCaptureBuffer.resize(bufferSize, 0.0f);
    reversePlaybackBuffer.resize(bufferSize, 0.0f);
    reverseCapturePos = 0;
    reversePlaybackPos = 0;
    reverseChunkSize = bufferSize / 2;  // Default chunk size
    reverseIsCapturing = true;
    reverseBufferReady = false;

    // Prepare pitch shifter
    pitchShifter.prepare(sr, bufferSize);
}

void DelayLine::reset()
{
    // Clear all samples in buffers
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePosition = 0;

    // Reset reverse delay state
    std::fill(reverseCaptureBuffer.begin(), reverseCaptureBuffer.end(), 0.0f);
    std::fill(reversePlaybackBuffer.begin(), reversePlaybackBuffer.end(), 0.0f);
    reverseCapturePos = 0;
    reversePlaybackPos = 0;
    reverseIsCapturing = true;
    reverseBufferReady = false;

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
        // CHUNK-BASED REVERSE DELAY:
        // Captures audio in chunks, reverses each chunk, then plays it back
        // This creates the classic "reverse delay" effect with periodic reversed snippets

        // Update chunk size based on current delay time
        reverseChunkSize = delaySamplesInt;
        if (reverseChunkSize <= 0) reverseChunkSize = 1;
        if (reverseChunkSize >= bufferSize) reverseChunkSize = bufferSize - 1;

        if (reverseIsCapturing)
        {
            // CAPTURING MODE: Record incoming audio into capture buffer
            reverseCaptureBuffer[reverseCapturePos] = input;
            reverseCapturePos++;

            // When capture buffer is full, reverse it and switch to playback mode
            if (reverseCapturePos >= reverseChunkSize)
            {
                // Reverse the captured audio into playback buffer
                for (int i = 0; i < reverseChunkSize; ++i)
                {
                    reversePlaybackBuffer[i] = reverseCaptureBuffer[reverseChunkSize - 1 - i];
                }

                // Switch to playback mode
                reverseIsCapturing = false;
                reversePlaybackPos = 0;
                reverseCapturePos = 0;
                reverseBufferReady = true;
            }

            // Output reversed audio if available, otherwise silence
            if (reverseBufferReady)
            {
                delayedSample = reversePlaybackBuffer[reversePlaybackPos];
                reversePlaybackPos++;

                // If we've played the entire reversed buffer, restart capturing
                if (reversePlaybackPos >= reverseChunkSize)
                {
                    reverseIsCapturing = true;
                    reversePlaybackPos = 0;
                    reverseCapturePos = 0;
                }
            }
            else
            {
                delayedSample = 0.0f;  // Silence during initial capture
            }
        }
        else
        {
            // PLAYBACK MODE: Play reversed audio while simultaneously capturing new audio
            // Play reversed chunk
            delayedSample = reversePlaybackBuffer[reversePlaybackPos];
            reversePlaybackPos++;

            // Simultaneously capture new audio
            reverseCaptureBuffer[reverseCapturePos] = input;
            reverseCapturePos++;

            // When playback finishes AND we've captured a full chunk, reverse and continue
            if (reversePlaybackPos >= reverseChunkSize && reverseCapturePos >= reverseChunkSize)
            {
                // Reverse the newly captured audio
                for (int i = 0; i < reverseChunkSize; ++i)
                {
                    reversePlaybackBuffer[i] = reverseCaptureBuffer[reverseChunkSize - 1 - i];
                }

                // Restart both playback and capture
                reversePlaybackPos = 0;
                reverseCapturePos = 0;
            }
        }
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
        reverseIsCapturing = true;
        reverseCapturePos = 0;
        reversePlaybackPos = 0;
        reverseBufferReady = false;
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
