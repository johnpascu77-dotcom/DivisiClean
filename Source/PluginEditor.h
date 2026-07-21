#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DivisiCleanAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit DivisiCleanAudioProcessorEditor(DivisiCleanAudioProcessor&);
    ~DivisiCleanAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DivisiCleanAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label cc31Label;
    juce::Label profileLabel;
    juce::Label typeLabel;
    juce::Label settingsLabel;
    juce::Label pendingLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DivisiCleanAudioProcessorEditor)
};
