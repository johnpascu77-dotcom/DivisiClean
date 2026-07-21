#include "PluginProcessor.h"
#include "PluginEditor.h"

DivisiCleanAudioProcessorEditor::DivisiCleanAudioProcessorEditor(DivisiCleanAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(420, 180);

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

    titleLabel.setBounds(area.removeFromTop(45));
    area.removeFromTop(10);

    cc31Label.setBounds(area.removeFromTop(35));
    profileLabel.setBounds(area.removeFromTop(35));
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
}
