#include "PluginProcessor.h"
#include "PluginEditor.h"

DivisiCleanAudioProcessorEditor::DivisiCleanAudioProcessorEditor(DivisiCleanAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(560, 395);

    titleLabel.setText("DivisiClean", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    cc31Label.setText("Active CC31: none", juce::dontSendNotification);
    cc31Label.setJustificationType(juce::Justification::centred);
    cc31Label.setFont(juce::FontOptions(18.0f));
    addAndMakeVisible(cc31Label);

    profileLabel.setText("Profile: No CC31 received", juce::dontSendNotification);
    profileLabel.setJustificationType(juce::Justification::centred);
    profileLabel.setFont(juce::FontOptions(16.0f));
    addAndMakeVisible(profileLabel);

    typeLabel.setText("Type: -", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(typeLabel);

    reductionModeLabel.setText("Reduction: -", juce::dontSendNotification);
    reductionModeLabel.setJustificationType(juce::Justification::centred);
    reductionModeLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(reductionModeLabel);

    divisiModeLabel.setText("Divisi mode: -", juce::dontSendNotification);
    divisiModeLabel.setJustificationType(juce::Justification::centred);
    divisiModeLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(divisiModeLabel);

    wrapModeLabel.setText("Wrap: -", juce::dontSendNotification);
    wrapModeLabel.setJustificationType(juce::Justification::centred);
    wrapModeLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(wrapModeLabel);

    settingsLabel.setText("Max voices: - | Window: - | Cap: -", juce::dontSendNotification);
    settingsLabel.setJustificationType(juce::Justification::centred);
    settingsLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(settingsLabel);

    pendingLabel.setText("Pending notes: 0", juce::dontSendNotification);
    pendingLabel.setJustificationType(juce::Justification::centred);
    pendingLabel.setFont(juce::FontOptions(15.0f));
    addAndMakeVisible(pendingLabel);
    jsonStatusLabel.setText("JSON: -", juce::dontSendNotification);
    jsonStatusLabel.setJustificationType(juce::Justification::centred);
    jsonStatusLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(jsonStatusLabel);

    startTimerHz(10);
}

DivisiCleanAudioProcessorEditor::~DivisiCleanAudioProcessorEditor()
{
    stopTimer();
}

void DivisiCleanAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff20252b));

    g.setColour(juce::Colour(0xff3a86ff));
    g.fillRoundedRectangle(20.0f,
        20.0f,
        static_cast<float> (getWidth() - 40),
        static_cast<float> (getHeight() - 40),
        12.0f);

    g.setColour(juce::Colour(0xff15191e));
    g.fillRoundedRectangle(26.0f,
        26.0f,
        static_cast<float> (getWidth() - 52),
        static_cast<float> (getHeight() - 52),
        10.0f);
}

void DivisiCleanAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(30);

    titleLabel.setBounds(area.removeFromTop(42));
    area.removeFromTop(8);

    cc31Label.setBounds(area.removeFromTop(30));
    profileLabel.setBounds(area.removeFromTop(28));
    typeLabel.setBounds(area.removeFromTop(26));
    reductionModeLabel.setBounds(area.removeFromTop(26));
    divisiModeLabel.setBounds(area.removeFromTop(26));
    wrapModeLabel.setBounds(area.removeFromTop(26));
    settingsLabel.setBounds(area.removeFromTop(26));
    pendingLabel.setBounds(area.removeFromTop(26));
    jsonStatusLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    reloadJsonButton.setBounds(area.removeFromTop(28).reduced(170, 0));
}

void DivisiCleanAudioProcessorEditor::timerCallback()
{
    const int cc31 = audioProcessor.getActiveCC31();

    if (cc31 < 0)
        cc31Label.setText("Active CC31: none", juce::dontSendNotification);
    else
        cc31Label.setText("Active CC31: " + juce::String(cc31), juce::dontSendNotification);

    profileLabel.setText("Profile: " + audioProcessor.getActiveProfileName(),
        juce::dontSendNotification);

    typeLabel.setText("Type: " + audioProcessor.getActiveProfileTypeName(),
        juce::dontSendNotification);

    reductionModeLabel.setText("Reduction: " + audioProcessor.getActiveSourceReductionModeName(),
        juce::dontSendNotification);

    divisiModeLabel.setText("Divisi mode: " + audioProcessor.getActiveExpectedDivisiModeName(),
        juce::dontSendNotification);

    const bool usesWindow = audioProcessor.getActiveProfileUsesChordWindow();
    const bool usesCap = audioProcessor.getActiveProfileEnforcesActiveVoiceLimit();

    juce::String windowText;

    if (usesWindow)
        windowText = juce::String(audioProcessor.getActiveProfileChordWindowMs(), 0) + " ms";
    else
        windowText = "OFF";

    wrapModeLabel.setText("Wrap: " + audioProcessor.getActiveRegisterWrapModeName(),
        juce::dontSendNotification);

    settingsLabel.setText(
        "Max voices: " + juce::String(audioProcessor.getActiveProfileMaxVoices())
        + " | Window: " + windowText
        + " | Cap: " + juce::String(usesCap ? "ON" : "OFF"),
        juce::dontSendNotification
    );

    pendingLabel.setText(
        "Pending notes: " + juce::String(audioProcessor.getPendingNoteCount()),
        juce::dontSendNotification
    );

    jsonStatusLabel.setText(
        "JSON: " + audioProcessor.getJsonProfileStatus(),
        juce::dontSendNotification
    );

    reloadJsonButton.setButtonText("Reload JSON + Panic");
    reloadJsonButton.setTooltip("Reloads JSON profiles and sends MIDI panic/reset at the next processing block. Best used while transport is stopped.");
    reloadJsonButton.onClick = [this]()
        {
            audioProcessor.reloadJsonProfilesFromGui();
        };
    addAndMakeVisible(reloadJsonButton);
}
