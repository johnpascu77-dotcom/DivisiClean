/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <algorithm>
#include <atomic>

//==============================================================================
/**
*/
enum class ProfileType
{
    PassThrough,
    SingleSource,
    SectionPoly,
    BlockVoicing
};

enum class SourceSelectionMode
{
    Highest,
    Lowest,
    ClosestToTarget
};

enum class SourceReductionMode
{
    None,
    LowestN,
    HighestN,
    Spread,
    ClosestToTarget,
    AsPlayed
};

enum class ExpectedDivisiMode
{
    None,
    BottomUp,
    TopDown,
    FillVoices
};

enum class RegisterWrapMode
{
    PerNoteNearTarget,
    PerVoiceRange
};

struct VoiceSourceRange
{
    int rank = 0;
    int minNote = 0;
    int maxNote = 127;
    int targetNote = 60;
    int outputTranspose = 0;
};

struct PresetProfile
{
    int cc31 = -1;
    juce::String name = "Unknown / Pass-through";

    ProfileType type = ProfileType::PassThrough;
    SourceSelectionMode selectionMode = SourceSelectionMode::Highest;
    SourceReductionMode reductionMode = SourceReductionMode::None;
    ExpectedDivisiMode expectedDivisiMode = ExpectedDivisiMode::None;
    RegisterWrapMode registerWrapMode = RegisterWrapMode::PerNoteNearTarget;

    int maxVoices = 16;
    int minNote = 0;
    int maxNote = 127;
    int targetNote = 60;
    int outputTranspose = 0;

    bool useChordWindow = false;
    double chordWindowMs = 0.0;
    bool enforceActiveVoiceLimit = false;

    std::vector<VoiceSourceRange> voiceSourceRanges;
};

class DivisiCleanAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    DivisiCleanAudioProcessor();
    ~DivisiCleanAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void loadJsonProfiles();
    void reloadJsonProfilesFromGui();
    juce::File getJsonProfilesFile() const;

    void requestMidiPanicAndStateReset();
    void addMidiPanicMessages(juce::MidiBuffer& outputMidi, int samplePosition) const;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    int getActiveCC31() const;
    juce::String getActiveProfileName() const;

    juce::String getActiveProfileTypeName() const;
    juce::String getActiveSourceReductionModeName() const;
    juce::String getActiveExpectedDivisiModeName() const;
    juce::String getActiveRegisterWrapModeName() const;
    juce::String getJsonProfileStatus() const;
    int getActiveProfileMaxVoices() const;
    double getActiveProfileChordWindowMs() const;
    bool getActiveProfileUsesChordWindow() const;
    bool getActiveProfileEnforcesActiveVoiceLimit() const;
    int getPendingNoteCount() const;

private:
    //==============================================================================
    struct PendingNote
    {
        int channel = 1;
        int noteNumber = 0;
        int velocity = 0;
        double ageMs = 0.0;
    };

    std::vector<PresetProfile> jsonProfiles;
    bool jsonProfilesLoaded = false;
    juce::String jsonProfileStatus = "JSON profiles not loaded";

    std::vector<PendingNote> pendingNotes;

    double samplesToMs(int numSamples) const;
    void flushPendingNotesIfReady(juce::MidiBuffer& outputMidi);
    void flushPendingNotesNow(juce::MidiBuffer& outputMidi);

    std::atomic<int> activeCC31{ -1 };
    std::atomic<bool> midiPanicAndResetRequested{ false };

    // Maps input notes to transformed output notes.
    // Index = (channel - 1) * 128 + inputNote.
    // Value:
    //   -1 = no mapping
    //   -2 = note-on was ignored/suppressed
    // 0-127 = output note number
    std::array<int, 16 * 128> activeNoteMap;

    PresetProfile getProfileForCC31(int cc31) const;
    PresetProfile getHardcodedProfileForCC31(int cc31) const;
    juce::String getProfileNameForCC31(int cc31) const;
    juce::String registerWrapModeToString(RegisterWrapMode mode) const;

    int getNoteMapIndex(int channel, int noteNumber) const;
    int wrapNoteNearTarget(int inputNote, int minNote, int maxNote, int targetNote) const;
    int chooseSingleSourceIndexFromNotes(const std::vector<int>& noteNumbers,
        const PresetProfile& profile) const;
    int jsonReloadCount = 0;
    int applyOutputTranspose(int noteNumber, int transposeSemitones) const;

    ProfileType profileTypeFromString(const juce::String& text) const;
    SourceSelectionMode sourceSelectionModeFromString(const juce::String& text) const;
    SourceReductionMode sourceReductionModeFromString(const juce::String& text) const;
    ExpectedDivisiMode expectedDivisiModeFromString(const juce::String& text) const;
    RegisterWrapMode registerWrapModeFromString(const juce::String& text) const;

    VoiceSourceRange getVoiceSourceRangeForProfileRank(const PresetProfile& profile,
        int rank,
        int totalRanks) const;

    std::vector<int> chooseBlockVoicingIndicesFromNotes(const std::vector<int>& noteNumbers,
        const PresetProfile& profile) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DivisiCleanAudioProcessor)

};
