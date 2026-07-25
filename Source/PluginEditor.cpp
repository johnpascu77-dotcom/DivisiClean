#include "PluginProcessor.h"



#include "PluginEditor.h"







DivisiCleanAudioProcessorEditor::DivisiCleanAudioProcessorEditor(DivisiCleanAudioProcessor& p)



    : AudioProcessorEditor(&p), audioProcessor(p)



{



    setSize(620, 500);







    titleLabel.setText("DivisiClean Lab ", juce::dontSendNotification);



    titleLabel.setJustificationType(juce::Justification::centred);



    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));



    addAndMakeVisible(titleLabel);







    modeLabel.setText("Mode:", juce::dontSendNotification);



    modeLabel.setJustificationType(juce::Justification::centredRight);



    modeLabel.setFont(juce::FontOptions(15.0f));



    addAndMakeVisible(modeLabel);







    modeComboBox.addItem("Chord Window", 1);



    modeComboBox.addItem("Bar Lookahead", 2);



    modeComboBox.addItem("Generated MIDI", 3);



    modeComboBox.addItem("ARP Driver", 4);



    modeComboBox.setSelectedId(static_cast<int>(audioProcessor.getEngineMode()) + 1,



        juce::dontSendNotification);



    modeComboBox.onChange = [this]()



        {



            const int selectedId = modeComboBox.getSelectedId();







            if (selectedId <= 0)



                return;







            audioProcessor.setEngineMode(static_cast<EngineMode>(selectedId - 1));

            audioProcessor.reloadJsonProfilesFromGui();



        };



    addAndMakeVisible(modeComboBox);







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







    engineStatusLabel.setText("Engine: -", juce::dontSendNotification);



    engineStatusLabel.setJustificationType(juce::Justification::centred);



    engineStatusLabel.setFont(juce::FontOptions(15.0f));



    addAndMakeVisible(engineStatusLabel);







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







    auto modeRow = area.removeFromTop(28);



    modeLabel.setBounds(modeRow.removeFromLeft(170));



    modeComboBox.setBounds(modeRow.removeFromLeft(220));



    area.removeFromTop(8);







    cc31Label.setBounds(area.removeFromTop(30));



    profileLabel.setBounds(area.removeFromTop(28));



    typeLabel.setBounds(area.removeFromTop(26));



    reductionModeLabel.setBounds(area.removeFromTop(26));



    divisiModeLabel.setBounds(area.removeFromTop(26));



    wrapModeLabel.setBounds(area.removeFromTop(26));



    engineStatusLabel.setBounds(area.removeFromTop(26));



    settingsLabel.setBounds(area.removeFromTop(26));



    pendingLabel.setBounds(area.removeFromTop(26));



    jsonStatusLabel.setBounds(area.removeFromTop(28));



    area.removeFromTop(6);



    reloadJsonButton.setBounds(area.removeFromTop(34).reduced(140, 2));



}







void DivisiCleanAudioProcessorEditor::timerCallback()



{



    const int desiredModeId = static_cast<int>(audioProcessor.getEngineMode()) + 1;







    if (modeComboBox.getSelectedId() != desiredModeId)



        modeComboBox.setSelectedId(desiredModeId, juce::dontSendNotification);







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







    engineStatusLabel.setText(audioProcessor.getEngineStatusText(),



    juce::dontSendNotification);







    settingsLabel.setText(



        "Max voices: " + juce::String(audioProcessor.getActiveProfileMaxVoices())



        + " | Window: " + windowText



        + " | Cap: " + juce::String(usesCap ? "ON" : "OFF"),



        juce::dontSendNotification



    );







    if (audioProcessor.getEngineTimingModeName() == "BarLookahead")



    {



        pendingLabel.setText(



            "Buffered notes: " + juce::String(audioProcessor.getBarLookaheadBufferedNoteCount())



            + " | Scheduled events: " + juce::String(audioProcessor.getBarLookaheadScheduledEventCount()),



            juce::dontSendNotification



        );



    }



    else
    {
        pendingLabel.setText(
            "Buffered notes: " + juce::String(audioProcessor.getBarLookaheadBufferedNoteCount())
            + " | Scheduled events: " + juce::String(audioProcessor.getBarLookaheadScheduledEventCount())
            + " | " + audioProcessor.getBarLookaheadDebugText(),
            juce::dontSendNotification
        );
    }

    jsonStatusLabel.setText(
        "JSON: " + audioProcessor.getJsonProfileStatus(),
        juce::dontSendNotification
    );




}



