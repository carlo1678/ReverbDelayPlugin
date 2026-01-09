/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void ReverbDelayPluginAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label,
    const juce::String& labelText,
    const juce::String& suffix)
{
    // ROTARY knob style (like a real pedal)
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

    // 270-degree rotation range
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
        juce::MathConstants<float>::pi * 2.8f,
        true);

    // APPLY CUSTOM PEDAL KNOB LOOK (THIS WAS MISSING!)
    slider.setLookAndFeel(&pedalKnobLAF);

    // Black and white text colors
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);

    slider.setTextValueSuffix(" " + suffix);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(label);
}

//==============================================================================
ReverbDelayPluginAudioProcessorEditor::ReverbDelayPluginAudioProcessorEditor(ReverbDelayPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set window size (increased to accommodate telephone controls)
    setSize(750, 750);

    // Setup Mix Slider (rotary knob)
    setupSlider(mixSlider, mixLabel, "MIX", "%");
    mixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "mix", mixSlider));

    // Setup Delay Time Slider (rotary knob) - shows note divisions or milliseconds
    setupSlider(delayTimeSlider, delayTimeLabel, "TIME", "");
    delayTimeAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_time", delayTimeSlider));

    // Configure to show note division or milliseconds based on time mode
    delayTimeSlider.textFromValueFunction = [this](double value)
        {
            // Get current time mode (0=Notes, 1=Time, 2=Triplet, 3=Dotted)
            int timeMode = static_cast<int>(audioProcessor.parameters.getRawParameterValue("time_mode")->load());

            if (timeMode == 1) // TIME mode - show milliseconds
            {
                return juce::String(static_cast<int>(value)) + " ms";
            }
            else // NOTES/TRIPLET/DOTTED modes - show note division
            {
                int index = static_cast<int>(value / 3000.0f);
                if (index > 4) index = 4;

                switch (index)
                {
                case 0: return juce::String("1/16");
                case 1: return juce::String("1/8");
                case 2: return juce::String("1/4");
                case 3: return juce::String("1/2");
                case 4: return juce::String("Whole");
                default: return juce::String("1/4");
                }
            }
        };

    // Setup Time Mode Dropdown
    timeModeBox.addItem("Notes", 1);
    timeModeBox.addItem("Time", 2);
    timeModeBox.addItem("Triplet", 3);
    timeModeBox.addItem("Dotted", 4);
    timeModeBox.setSelectedId(1); // Default to Notes
    timeModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    timeModeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    timeModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    timeModeBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);

    // Update delay time slider text when mode changes
    timeModeBox.onChange = [this]
    {
        delayTimeSlider.updateText(); // Force update of the displayed text
    };

    addAndMakeVisible(timeModeBox);
    timeModeAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        audioProcessor.parameters, "time_mode", timeModeBox));

    timeModeLabel.setText("MODE", juce::dontSendNotification);
    timeModeLabel.setJustificationType(juce::Justification::centred);
    timeModeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeModeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    timeModeLabel.attachToComponent(&timeModeBox, false);
    addAndMakeVisible(timeModeLabel);

    // Setup Pendulum Speed selector
    pendulumSpeedBox.addItem("1 Bar", 1);
    pendulumSpeedBox.addItem("1/2 Bar", 2);
    pendulumSpeedBox.addItem("1/4 Bar", 3);
    pendulumSpeedBox.addItem("1/8 Bar", 4);
    pendulumSpeedBox.addItem("1/16 Bar", 5);
    pendulumSpeedBox.setSelectedId(3); // Default to 1/4 Bar
    pendulumSpeedBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    pendulumSpeedBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    pendulumSpeedBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    pendulumSpeedBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(pendulumSpeedBox);
    pendulumSpeedAttachment.reset(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(
        audioProcessor.parameters, "pendulum_speed", pendulumSpeedBox));

    pendulumSpeedLabel.setText("PENDULUM SPEED", juce::dontSendNotification);
    pendulumSpeedLabel.setJustificationType(juce::Justification::centred);
    pendulumSpeedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    pendulumSpeedLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(pendulumSpeedLabel);

    // Initially hide pendulum speed control (shown when pendulum pan is enabled)
    pendulumSpeedBox.setVisible(false);
    pendulumSpeedLabel.setVisible(false);

    // Setup Preset selector
    auto presetNames = audioProcessor.getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetBox.addItem(presetNames[i], i + 1); // IDs start at 1
    }
    presetBox.setSelectedId(0); // No preset selected by default
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    presetBox.onChange = [this]
    {
        int selectedId = presetBox.getSelectedId();
        if (selectedId > 0)
        {
            audioProcessor.loadPreset(selectedId - 1); // Convert ID back to index

            // Show/hide preset-specific controls
            bool isTelephonePreset = (selectedId == 1); // Telephone is first item (ID 1)
            bool isUnderwaterPreset = (selectedId == 2); // Underwater is second item (ID 2)
            bool isTapePreset = (selectedId == 3); // Tape is third item (ID 3)

            // Telephone preset controls
            noiseSlider.setVisible(isTelephonePreset);
            phaserMixSlider.setVisible(isTelephonePreset);
            phaserSpeedSlider.setVisible(isTelephonePreset);

            // Underwater preset controls
            underwaterMixSlider.setVisible(isUnderwaterPreset);

            // Tape preset controls (wow, flutter, mechanical noise in Preset Specific EFX box)
            // When tape is selected, wow/flutter move from MOD box to Preset Specific EFX box
            wowSlider.setVisible(true); // Always visible (in MOD box or Preset EFX box)
            flutterSlider.setVisible(true); // Always visible (in MOD box or Preset EFX box)
            mechanicalNoiseSlider.setVisible(isTapePreset);

            // Show Preset Specific EFX box if any preset-specific controls or pendulum pan is active
            bool isPendulumEnabled = pendulumPanButton.getToggleState();
            presetEfxLabel.setVisible(isTelephonePreset || isUnderwaterPreset || isTapePreset || isPendulumEnabled);

            // Trigger layout update
            resized();
        }
    };
    addAndMakeVisible(presetBox);

    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    presetLabel.attachToComponent(&presetBox, false);
    addAndMakeVisible(presetLabel);

    // Setup Feedback Slider (rotary knob)
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK", "");
    delayFeedbackAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "delay_feedback", delayFeedbackSlider));

    // Setup Low Cut Slider (rotary knob)
    setupSlider(lowCutSlider, lowCutLabel, "LOW CUT", " Hz");
    lowCutAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "low_cut", lowCutSlider));

    // Setup High Cut Slider (rotary knob)
    setupSlider(highCutSlider, highCutLabel, "HIGH CUT", " Hz");
    highCutAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "high_cut", highCutSlider));

    // Setup Wow Slider (rotary knob)
    setupSlider(wowSlider, wowLabel, "WOW", "%");
    wowAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "wow", wowSlider));

    // Setup Flutter Slider (rotary knob)
    setupSlider(flutterSlider, flutterLabel, "FLUTTER", "%");
    flutterAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "flutter", flutterSlider));

    // Setup MOD label
    modLabel.setText("MOD", juce::dontSendNotification);
    modLabel.setJustificationType(juce::Justification::centred);
    modLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    modLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(modLabel);

    // Setup Preset Specific EFX label
    presetEfxLabel.setText("Preset Specific EFX", juce::dontSendNotification);
    presetEfxLabel.setJustificationType(juce::Justification::centred);
    presetEfxLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    presetEfxLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(presetEfxLabel);
    presetEfxLabel.setVisible(false); // Hidden by default

    // Setup Reverse Button
    reverseDelayButton.setButtonText("REVERSE");
    reverseDelayButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    reverseDelayButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    reverseDelayButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(reverseDelayButton);
    reverseDelayAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "reverse_delay", reverseDelayButton));

    reverseDelayAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "reverse_delay", reverseDelayButton));

    // Setup Ping Pong Button
    pingPongButton.setButtonText("PING PONG");
    pingPongButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    pingPongButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    pingPongButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(pingPongButton);
    pingPongAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "ping_pong", pingPongButton));

    // Setup Pendulum Pan Button
    pendulumPanButton.setButtonText("PENDULUM PAN");
    pendulumPanButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    pendulumPanButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    pendulumPanButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    addAndMakeVisible(pendulumPanButton);
    pendulumPanAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(
        audioProcessor.parameters, "pendulum_pan", pendulumPanButton));

    // Show/hide pendulum speed control when pendulum pan is toggled
    pendulumPanButton.onClick = [this]
    {
        bool isPendulumEnabled = pendulumPanButton.getToggleState();
        pendulumSpeedBox.setVisible(isPendulumEnabled);
        pendulumSpeedLabel.setVisible(isPendulumEnabled);

        // Show Preset Specific EFX box if pendulum is enabled or if preset-specific controls are visible
        int selectedId = presetBox.getSelectedId();
        bool isTelephonePreset = (selectedId == 1);
        bool isUnderwaterPreset = (selectedId == 2);
        bool isTapePreset = (selectedId == 3);
        presetEfxLabel.setVisible(isTelephonePreset || isUnderwaterPreset || isTapePreset || isPendulumEnabled);

        // Trigger layout update
        resized();
    };

    // Setup Noise Slider (telephone preset only)
    setupSlider(noiseSlider, noiseLabel, "NOISE", "%");
    noiseAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "noise", noiseSlider));
    noiseSlider.setVisible(false); // Hidden by default

    // Setup Phaser Mix Slider (telephone preset only)
    setupSlider(phaserMixSlider, phaserMixLabel, "PHASER MIX", "%");
    phaserMixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "phaser_mix", phaserMixSlider));
    phaserMixSlider.setVisible(false); // Hidden by default

    // Setup Phaser Speed Slider (telephone preset only)
    setupSlider(phaserSpeedSlider, phaserSpeedLabel, "PHASER SPEED", " Hz");
    phaserSpeedAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "phaser_speed", phaserSpeedSlider));
    phaserSpeedSlider.setVisible(false); // Hidden by default

    // Setup Underwater Mix Slider (underwater preset only) - labeled as "Bubbles"
    setupSlider(underwaterMixSlider, underwaterMixLabel, "BUBBLES", "%");
    underwaterMixAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "underwater_mix", underwaterMixSlider));
    underwaterMixSlider.setVisible(false); // Hidden by default

    // Setup Mechanical Noise Slider (tape preset only)
    setupSlider(mechanicalNoiseSlider, mechanicalNoiseLabel, "MECHANICAL", "%");
    mechanicalNoiseAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "mechanical_noise", mechanicalNoiseSlider));
    mechanicalNoiseSlider.setVisible(false); // Hidden by default

    // Setup Reset Button
    resetButton.setButtonText("RESET");
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    resetButton.onClick = [this]
    {
        // Reset all parameters to default values
        audioProcessor.parameters.getParameter("mix")->setValueNotifyingHost(0.50f); // 50%
        audioProcessor.parameters.getParameter("delay_time")->setValueNotifyingHost(500.0f / 15000.0f);
        audioProcessor.parameters.getParameter("time_mode")->setValueNotifyingHost(0.0f); // Notes
        audioProcessor.parameters.getParameter("delay_feedback")->setValueNotifyingHost(0.30f);
        audioProcessor.parameters.getParameter("tempo_sync")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("reverse_delay")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("ping_pong")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("low_cut")->setValueNotifyingHost(0.0f); // 20 Hz
        audioProcessor.parameters.getParameter("high_cut")->setValueNotifyingHost(1.0f); // 20000 Hz
        audioProcessor.parameters.getParameter("wow")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("flutter")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("pendulum_pan")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("noise")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("phaser_mix")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("phaser_speed")->setValueNotifyingHost(1.0f / 10.0f);
        audioProcessor.parameters.getParameter("underwater_mix")->setValueNotifyingHost(0.0f);
        audioProcessor.parameters.getParameter("mechanical_noise")->setValueNotifyingHost(0.0f);

        // Reset preset selector
        presetBox.setSelectedId(0);

        // Hide preset-specific controls
        noiseSlider.setVisible(false);
        phaserMixSlider.setVisible(false);
        phaserSpeedSlider.setVisible(false);
        underwaterMixSlider.setVisible(false);
        mechanicalNoiseSlider.setVisible(false);
        wowSlider.setVisible(true); // Show wow/flutter by default (not in EFX box)
        flutterSlider.setVisible(true);
        presetEfxLabel.setVisible(false);
    };
    addAndMakeVisible(resetButton);
}



ReverbDelayPluginAudioProcessorEditor::~ReverbDelayPluginAudioProcessorEditor()
{
    // Reset LookAndFeel for all sliders (IMPORTANT!)
    mixSlider.setLookAndFeel(nullptr);
    delayTimeSlider.setLookAndFeel(nullptr);
    delayFeedbackSlider.setLookAndFeel(nullptr);
    lowCutSlider.setLookAndFeel(nullptr);
    highCutSlider.setLookAndFeel(nullptr);
    wowSlider.setLookAndFeel(nullptr);
    flutterSlider.setLookAndFeel(nullptr);
    noiseSlider.setLookAndFeel(nullptr);
    phaserMixSlider.setLookAndFeel(nullptr);
    phaserSpeedSlider.setLookAndFeel(nullptr);
    underwaterMixSlider.setLookAndFeel(nullptr);
    mechanicalNoiseSlider.setLookAndFeel(nullptr);
}

//==============================================================================
void ReverbDelayPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Black background
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds();

    // Draw title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText("The Lockboxx Effect", bounds.removeFromTop(60), juce::Justification::centred);

    // Draw MOD box border
    // Calculate mod box position (same logic as in resized())
    auto modBoxBounds = getLocalBounds();
    modBoxBounds.reduce(30, 30);
    modBoxBounds.removeFromTop(60 + 10 + 120 + 20); // Skip title, top row spacing

    auto middleRow = modBoxBounds.removeFromTop(180);
    int middleSectionWidth = middleRow.getWidth() / 3;
    middleRow.removeFromLeft(middleSectionWidth); // Skip Low Cut section

    auto modBoxSection = middleRow.removeFromLeft(middleSectionWidth);
    modBoxSection.reduce(10, 10); // Add some padding

    // Draw rounded rectangle border for MOD box
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(modBoxSection.toFloat(), 8.0f, 2.0f);

    // Draw Preset Specific EFX box border (only when visible)
    if (presetEfxLabel.isVisible())
    {
        auto presetEfxBoxBounds = getLocalBounds();
        presetEfxBoxBounds.reduce(30, 30);
        presetEfxBoxBounds.removeFromTop(60 + 10 + 120 + 20 + 180 + 10 + 55 + 10); // Skip all previous sections

        auto presetEfxBoxSection = presetEfxBoxBounds.removeFromTop(160);
        presetEfxBoxSection.reduce(10, 10); // Add some padding

        // Draw rounded rectangle border for Preset Specific EFX box
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(presetEfxBoxSection.toFloat(), 8.0f, 2.0f);
    }
}

void ReverbDelayPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(30, 30);

    // Title space
    auto titleArea = bounds.removeFromTop(60);

    // Preset selector and reset button in top right corner
    auto topRightArea = titleArea.removeFromRight(320);
    topRightArea.removeFromTop(25); // Space for "PRESET" label

    // Reset button on the left
    auto resetButtonArea = topRightArea.removeFromLeft(110);
    resetButton.setBounds(resetButtonArea.withSizeKeepingCentre(90, 30));

    topRightArea.removeFromLeft(10); // Spacing

    // Preset selector on the right
    presetBox.setBounds(topRightArea.withSizeKeepingCentre(180, 30));

    bounds.removeFromTop(10);

    // Top row: TIME - MIX - FEEDBACK
    auto topRow = bounds.removeFromTop(120);
    int topSectionWidth = topRow.getWidth() / 3;

    // TIME knob (left section)
    delayTimeSlider.setBounds(topRow.removeFromLeft(topSectionWidth).reduced(20));

    // MIX knob (center section)
    auto mixSection = topRow.removeFromLeft(topSectionWidth);
    mixSlider.setBounds(mixSection.withSizeKeepingCentre(110, 110));

    // FEEDBACK knob (right section)
    delayFeedbackSlider.setBounds(topRow.reduced(20));

    bounds.removeFromTop(20);

    // Middle row: Low Cut - MOD BOX - High Cut
    auto middleRow = bounds.removeFromTop(180);
    int middleSectionWidth = middleRow.getWidth() / 3;

    // Low Cut knob (left section)
    auto lowCutSection = middleRow.removeFromLeft(middleSectionWidth);
    lowCutSlider.setBounds(lowCutSection.removeFromTop(120).reduced(20));

    // MOD BOX (center section)
    auto modBoxSection = middleRow.removeFromLeft(middleSectionWidth);

    // MOD title label at top of box (with spacing from border)
    modBoxSection.removeFromTop(5); // Add spacing between border and label
    modLabel.setBounds(modBoxSection.removeFromTop(30));

    // Mod knobs side by side inside the box
    // Only show wow/flutter here if NOT in tape preset (tape preset shows them in Preset Specific EFX box)
    if (!mechanicalNoiseSlider.isVisible())
    {
        auto modKnobsArea = modBoxSection.removeFromTop(110);
        int modKnobWidth = modKnobsArea.getWidth() / 2;

        // Wow knob (left side of mod box)
        wowSlider.setBounds(modKnobsArea.removeFromLeft(modKnobWidth).reduced(10));

        // Flutter knob (right side of mod box)
        flutterSlider.setBounds(modKnobsArea.reduced(10));
    }

    // High Cut knob (right section)
    auto highCutSection = middleRow;
    highCutSlider.setBounds(highCutSection.removeFromTop(120).reduced(20));

    bounds.removeFromTop(10);

    // MODE dropdown - centered below mod box
    auto modeRow = bounds.removeFromTop(55);
    modeRow.removeFromTop(25); // Space for "MODE" label
    timeModeBox.setBounds(modeRow.withSizeKeepingCentre(120, 30));

    bounds.removeFromTop(10);

    // Preset Specific EFX box (only visible when preset-specific controls are active)
    if (presetEfxLabel.isVisible())
    {
        auto presetEfxRow = bounds.removeFromTop(160);

        // Preset Specific EFX title label at top of box (with spacing from border)
        presetEfxRow.removeFromTop(5); // Add spacing between border and label
        presetEfxLabel.setBounds(presetEfxRow.removeFromTop(30));

        // Content area inside the box
        auto presetEfxContentArea = presetEfxRow.removeFromTop(110);

        // Telephone controls (3 knobs side by side)
        if (noiseSlider.isVisible())
        {
            int telephoneKnobWidth = presetEfxContentArea.getWidth() / 3;

            // Noise knob (left third)
            noiseSlider.setBounds(presetEfxContentArea.removeFromLeft(telephoneKnobWidth).reduced(15));

            // Phaser Mix knob (center third)
            phaserMixSlider.setBounds(presetEfxContentArea.removeFromLeft(telephoneKnobWidth).reduced(15));

            // Phaser Speed knob (right third)
            phaserSpeedSlider.setBounds(presetEfxContentArea.reduced(15));
        }

        // Underwater controls (1 knob centered)
        if (underwaterMixSlider.isVisible())
        {
            // Bubbles knob (centered)
            underwaterMixSlider.setBounds(presetEfxContentArea.withSizeKeepingCentre(120, 110));
        }

        // Tape controls (3 knobs side by side: wow, flutter, mechanical noise)
        if (wowSlider.isVisible() && mechanicalNoiseSlider.isVisible())
        {
            int tapeKnobWidth = presetEfxContentArea.getWidth() / 3;

            // Wow knob (left third)
            wowSlider.setBounds(presetEfxContentArea.removeFromLeft(tapeKnobWidth).reduced(15));

            // Flutter knob (center third)
            flutterSlider.setBounds(presetEfxContentArea.removeFromLeft(tapeKnobWidth).reduced(15));

            // Mechanical Noise knob (right third)
            mechanicalNoiseSlider.setBounds(presetEfxContentArea.reduced(15));
        }

        // Pendulum speed control (centered dropdown)
        if (pendulumSpeedBox.isVisible())
        {
            // Center the pendulum speed dropdown
            auto pendulumSpeedArea = presetEfxContentArea.withSizeKeepingCentre(180, 40);
            pendulumSpeedLabel.setBounds(pendulumSpeedArea.removeFromTop(20));
            pendulumSpeedBox.setBounds(pendulumSpeedArea);
        }

        bounds.removeFromTop(10);
    }

    // Bottom row: PING PONG - PENDULUM PAN - REVERSE
    auto bottomRow = bounds.removeFromTop(70);
    int buttonWidth = bottomRow.getWidth() / 3;

    // Ping Pong button (left third)
    auto pingPongButtonArea = bottomRow.removeFromLeft(buttonWidth);
    pingPongButton.setBounds(pingPongButtonArea.withSizeKeepingCentre(120, 28));

    // Pendulum Pan button (center third)
    auto pendulumPanButtonArea = bottomRow.removeFromLeft(buttonWidth);
    pendulumPanButton.setBounds(pendulumPanButtonArea.withSizeKeepingCentre(140, 28));

    // Reverse button (right third)
    reverseDelayButton.setBounds(bottomRow.withSizeKeepingCentre(120, 28));
}