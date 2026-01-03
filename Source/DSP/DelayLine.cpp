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
    reverseCaptureBuffer.resize(bufferSize, 0.0f);
    reversePlaybackBuffer.resize(bufferSize, 0.0f);
    writePosition = 0;
    reversePhase = 0;
    lastSnapshotSize = 0;

    // Prepare pitch shifter
    pitchShifter.prepare(sr, bufferSize);
}

void DelayLine::reset()
{
    // Clear all samples in buffers
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    std::fill(reverseCaptureBuffer.begin(), reverseCaptureBuffer.end(), 0.0f);
    std::fill(reversePlaybackBuffer.begin(), reversePlaybackBuffer.end(), 0.0f);
    writePosition = 0;
    reversePhase = 0;
    lastSnapshotSize = 0;

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
        // SNAPSHOT-BASED REVERSE DELAY:
        // Captures audio in rhythmic segments equal to delay time, then plays each segment reversed

        // Step 1: Read the normally-delayed audio (what we'll capture)
        int normalReadPos = writePosition - delaySamplesInt;
        if (normalReadPos < 0)
            normalReadPos += bufferSize;
        float capturedSample = buffer[normalReadPos];

        // Step 2: Store captured sample in capture buffer at current phase position
        if (reversePhase < bufferSize)
            reverseCaptureBuffer[reversePhase] = capturedSample;

        // Step 3: Play back from playback buffer in REVERSE
        // Read from (lastSnapshotSize - 1 - reversePhase) to play the snapshot backwards
        if (lastSnapshotSize > 0 && reversePhase < lastSnapshotSize)
        {
            int playbackPos = lastSnapshotSize - 1 - reversePhase;
            if (playbackPos >= 0 && playbackPos < bufferSize)
                delayedSample = reversePlaybackBuffer[playbackPos];
            else
                delayedSample = 0.0f;
        }
        else
        {
            delayedSample = 0.0f;  // Silence during first snapshot period
        }

        // Step 4: Increment phase
        reversePhase++;

        // Step 5: When snapshot period completes, swap buffers and start new period
        if (reversePhase >= delaySamplesInt)
        {
            // Copy capture buffer to playback buffer for next period
            for (int i = 0; i < delaySamplesInt && i < bufferSize; ++i)
            {
                reversePlaybackBuffer[i] = reverseCaptureBuffer[i];
            }

            lastSnapshotSize = delaySamplesInt;  // Save snapshot size
            reversePhase = 0;  // Reset for next snapshot period
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
        reversePhase = 0;
        lastSnapshotSize = 0;
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
