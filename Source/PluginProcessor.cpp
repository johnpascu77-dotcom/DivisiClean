/*

  ==============================================================================



    This file contains the basic framework code for a JUCE plugin processor.



  ==============================================================================

*/



#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <map>

#include <cmath>

#include <algorithm>



//==============================================================================

DivisiCleanAudioProcessor::DivisiCleanAudioProcessor()

#ifndef JucePlugin_PreferredChannelConfigurations

     : AudioProcessor (BusesProperties()

                     #if ! JucePlugin_IsMidiEffect

                      #if ! JucePlugin_IsSynth

                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)

                      #endif

                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)

                     #endif

                       )

#endif

{

    activeNoteMap.fill(-1);

    ensureRuntimeJsonProfilesExist();
    loadJsonProfiles();

}



DivisiCleanAudioProcessor::~DivisiCleanAudioProcessor()

{

}



juce::String DivisiCleanAudioProcessor::getJsonProfilesFileNameForCurrentMode() const

{

    switch (getEngineMode())

    {

    case EngineMode::ChordWindow:

        return "DivisiCleanProfiles_ChordWindow.json";



    case EngineMode::BarLookahead:

        return "DivisiCleanProfiles_BarLookahead.json";



    case EngineMode::GeneratedMidi:

        return "DivisiCleanProfiles_GeneratedMidi.json";



    case EngineMode::ArpDriver:

        return "DivisiCleanProfiles_ArpDriver.json";

    }



    return "DivisiCleanProfiles.json";

}



void DivisiCleanAudioProcessor::ensureRuntimeJsonProfilesExist()
{
    const auto runtimeProfilesDir = getRuntimeProfilesDirectory();

    if (! runtimeProfilesDir.exists())
        runtimeProfilesDir.createDirectory();

    copyFactoryProfileIfMissing("DivisiCleanProfiles_ChordWindow.json");
    copyFactoryProfileIfMissing("DivisiCleanProfiles_BarLookahead.json");
    copyFactoryProfileIfMissing("DivisiCleanProfiles_GeneratedMidi.json");
    copyFactoryProfileIfMissing("DivisiCleanProfiles_ArpDriver.json");
    copyFactoryProfileIfMissing("DivisiCleanProfiles.json");
}

juce::File DivisiCleanAudioProcessor::getRuntimeProfilesDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("DivisiClean")
        .getChildFile("Profiles");
}

juce::File DivisiCleanAudioProcessor::getFactoryProfilesDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    for (int i = 0; i < 8; ++i)
    {
        const auto candidate = dir.getChildFile("Resources").getChildFile("Profiles");

        if (candidate.isDirectory())
            return candidate;

        const auto parent = dir.getParentDirectory();

        if (parent == dir)
            break;

        dir = parent;
    }

    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile("Resources")
        .getChildFile("Profiles");
}

bool DivisiCleanAudioProcessor::copyFactoryProfileIfMissing(const juce::String& fileName)
{
    const auto runtimeFile = getRuntimeProfilesDirectory().getChildFile(fileName);

    if (runtimeFile.existsAsFile())
        return true;

    const auto factoryFile = getFactoryProfilesDirectory().getChildFile(fileName);

    if (! factoryFile.existsAsFile())
        return false;

    runtimeFile.getParentDirectory().createDirectory();
    return factoryFile.copyFileTo(runtimeFile);
}

juce::File DivisiCleanAudioProcessor::getJsonProfilesFile() const

{

    const auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    const auto rootDir = documentsDir.getChildFile("DivisiClean");

    const auto profilesDir = rootDir.getChildFile("Profiles");



    const auto modeSpecificFile = profilesDir.getChildFile(getJsonProfilesFileNameForCurrentMode());



    if (modeSpecificFile.existsAsFile())

        return modeSpecificFile;



    const auto profilesDirFallback = profilesDir.getChildFile("DivisiCleanProfiles.json");



    if (profilesDirFallback.existsAsFile())

        return profilesDirFallback;



    return rootDir.getChildFile("DivisiCleanProfiles.json");

}



static juce::String normaliseEnumText(juce::String text)

{

    text = text.trim();

    text = text.removeCharacters(" _-");

    return text.toLowerCase();

}



ProfileType DivisiCleanAudioProcessor::profileTypeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "singlesource")

        return ProfileType::SingleSource;



    if (t == "sectionpoly")

        return ProfileType::SectionPoly;



    if (t == "blockvoicing")

        return ProfileType::BlockVoicing;



    return ProfileType::PassThrough;

}



SourceSelectionMode DivisiCleanAudioProcessor::sourceSelectionModeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "lowest")

        return SourceSelectionMode::Lowest;



    if (t == "closesttotarget")

        return SourceSelectionMode::ClosestToTarget;



    return SourceSelectionMode::Highest;

}



SourceReductionMode DivisiCleanAudioProcessor::sourceReductionModeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "lowestn")

        return SourceReductionMode::LowestN;



    if (t == "highestn")

        return SourceReductionMode::HighestN;



    if (t == "spread")

        return SourceReductionMode::Spread;



    if (t == "closesttotarget")

        return SourceReductionMode::ClosestToTarget;



    if (t == "asplayed")

        return SourceReductionMode::AsPlayed;



    return SourceReductionMode::None;

}



ExpectedDivisiMode DivisiCleanAudioProcessor::expectedDivisiModeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "bottomup")

        return ExpectedDivisiMode::BottomUp;



    if (t == "topdown")

        return ExpectedDivisiMode::TopDown;



    if (t == "fillvoices")

        return ExpectedDivisiMode::FillVoices;



    return ExpectedDivisiMode::None;

}



RegisterWrapMode DivisiCleanAudioProcessor::registerWrapModeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "pervoicerange")

        return RegisterWrapMode::PerVoiceRange;



    return RegisterWrapMode::PerNoteNearTarget;

}



TimingMode DivisiCleanAudioProcessor::timingModeFromString(const juce::String& text) const

{

    const auto t = normaliseEnumText(text);



    if (t == "barlookahead")

        return TimingMode::BarLookahead;



    return TimingMode::ChordWindow;

}



void DivisiCleanAudioProcessor::loadEngineSettingsFromJsonRoot(const juce::DynamicObject& root)

{

    engineSettings = EngineTimingSettings{};



    const auto engineVar = root.getProperty("engineSettings");



    if (!engineVar.isObject())

        return;



    const auto* engineObject = engineVar.getDynamicObject();



    if (engineObject == nullptr)

        return;



    engineSettings.timingMode =

        timingModeFromString(engineObject->getProperty("timingMode").toString());



    engineSettings.enabled =

        static_cast<bool>(engineObject->getProperty("enabled"));



    const auto quantizeVar = engineObject->getProperty("quantizeDivisionsPerBar");

    if (!quantizeVar.isVoid())

        engineSettings.quantizeDivisionsPerBar =

            juce::jlimit(1, 128, static_cast<int>(quantizeVar));



    const auto minLenVar = engineObject->getProperty("minNoteLengthBeats");

    if (!minLenVar.isVoid())

        engineSettings.minNoteLengthBeats =

            juce::jlimit(0.0, 4.0, static_cast<double>(minLenVar));



    const auto mergeGapVar = engineObject->getProperty("mergeGapBeats");

    if (!mergeGapVar.isVoid())

        engineSettings.mergeGapBeats =

            juce::jlimit(0.0, 4.0, static_cast<double>(mergeGapVar));



    const auto delayVar = engineObject->getProperty("outputDelayBars");

    if (!delayVar.isVoid())

        engineSettings.outputDelayBars =

            juce::jlimit(0.0, 8.0, static_cast<double>(delayVar));



    const auto suppressVar = engineObject->getProperty("suppressShortNotes");

    if (!suppressVar.isVoid())

        engineSettings.suppressShortNotes =

            static_cast<bool>(suppressVar);



    const auto mergeVar = engineObject->getProperty("mergeRepeatedNotes");

    if (!mergeVar.isVoid())

        engineSettings.mergeRepeatedNotes =

            static_cast<bool>(mergeVar);



    const auto stopVar = engineObject->getProperty("resetOnTransportStop");

    if (!stopVar.isVoid())

        engineSettings.resetOnTransportStop =

            static_cast<bool>(stopVar);



    const auto loopVar = engineObject->getProperty("resetOnLoopJump");

    if (!loopVar.isVoid())

        engineSettings.resetOnLoopJump =

            static_cast<bool>(loopVar);

}



void DivisiCleanAudioProcessor::loadJsonProfiles()

{

    jsonProfiles.clear();

    jsonProfilesLoaded = false;



    const auto jsonFile = getJsonProfilesFile();

    const auto makeJsonStatus = [this, &jsonFile](const juce::String& state,
        const juce::String& message,
        int profileCount = -1)
        {
            juce::String status;

            status << "JSON " << state
                << " | Mode: " << getEngineModeDisplayName()
                << " | Enum: " << juce::String(static_cast<int>(getEngineMode()))
                << " | File: " << jsonFile.getFileName();

            if (profileCount >= 0)
                status << " | Profiles: " << juce::String(profileCount);

            status << " | " << message
                << " | Path: " << jsonFile.getFullPathName();

            return status;
        };



    if (!jsonFile.existsAsFile())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "File not found, using built-in profiles");

        return;

    }



    const auto jsonText = jsonFile.loadFileAsString();



    if (jsonText.trim().isEmpty())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "File is empty, using built-in profiles");

        return;

    }



    juce::var parsedJson;

    const auto parseResult = juce::JSON::parse(jsonText, parsedJson);



    if (parseResult.failed())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "Parse failed: " + parseResult.getErrorMessage());

        return;

    }



    if (!parsedJson.isObject())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "Root is not an object, using built-in profiles");

        return;

    }



    const auto* root = parsedJson.getDynamicObject();



    if (root == nullptr)

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "Root object invalid, using built-in profiles");

        return;

    }



    loadEngineSettingsFromJsonRoot(*root);



    // Keep the selected engine mode as the source of truth.

    // JSON engineSettings may configure timing parameters, but must not

    // silently desynchronise engineSettings.timingMode from selectedEngineMode.

    setEngineMode(getEngineMode());



    const auto profilesVar = root->getProperty("profiles");



    if (!profilesVar.isArray())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "profiles field missing or not an array");

        return;

    }



    const auto* profilesArray = profilesVar.getArray();



    if (profilesArray == nullptr || profilesArray->isEmpty())

    {

        jsonProfileStatus = makeJsonStatus("ERROR", "profiles array is empty, using built-in profiles");

        return;

    }


    std::vector<PresetProfile> loadedProfiles;

    loadedProfiles.reserve(static_cast<size_t>(profilesArray->size()));

    int skippedProfileCount = 0;
    juce::StringArray validationWarnings;

    const auto addValidationWarning = [&validationWarnings](const juce::String& warning)
        {
            if (validationWarnings.size() < 5)
                validationWarnings.add(warning);
        };

    const auto hasRequiredProperty = [](const juce::DynamicObject& object,
        const juce::String& propertyName)
        {
            return object.hasProperty(juce::Identifier(propertyName));
        };

    const auto validateRequiredProfileFields = [&hasRequiredProperty](const juce::DynamicObject& object,
        juce::String& error)
        {
            static const char* requiredFields[] =
            {
                "cc31",
                "name",
                "profileType",
                "sourceSelectionMode",
                "sourceReductionMode",
                "expectedDivisiMode",
                "registerWrapMode",
                "maxVoices",
                "minNote",
                "maxNote",
                "targetNote",
                "outputTranspose",
                "useChordWindow",
                "chordWindowMs",
                "enforceActiveVoiceLimit"
            };

            for (const auto* field : requiredFields)
            {
                if (!hasRequiredProperty(object, field))
                {
                    error = "missing required field '" + juce::String(field) + "'";
                    return false;
                }
            }

            return true;
        };



    for (int profileIndex = 0; profileIndex < profilesArray->size(); ++profileIndex)

    {

        const auto& profileVar = profilesArray->getReference(profileIndex);

        if (!profileVar.isObject())
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] is not an object");
            continue;
        }



        const auto* profileObject = profileVar.getDynamicObject();



        if (profileObject == nullptr)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] object invalid");
            continue;
        }

        juce::String validationError;

        if (!validateRequiredProfileFields(*profileObject, validationError))
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] " + validationError);
            continue;
        }



        PresetProfile profile;



        profile.cc31 = static_cast<int>(profileObject->getProperty("cc31"));

        profile.name = profileObject->getProperty("name").toString();



        profile.type = profileTypeFromString(profileObject->getProperty("profileType").toString());

        profile.selectionMode = sourceSelectionModeFromString(profileObject->getProperty("sourceSelectionMode").toString());

        profile.reductionMode = sourceReductionModeFromString(profileObject->getProperty("sourceReductionMode").toString());

        profile.expectedDivisiMode = expectedDivisiModeFromString(profileObject->getProperty("expectedDivisiMode").toString());

        profile.registerWrapMode = registerWrapModeFromString(profileObject->getProperty("registerWrapMode").toString());



        profile.maxVoices = static_cast<int>(profileObject->getProperty("maxVoices"));

        profile.minNote = static_cast<int>(profileObject->getProperty("minNote"));

        profile.maxNote = static_cast<int>(profileObject->getProperty("maxNote"));

        profile.targetNote = static_cast<int>(profileObject->getProperty("targetNote"));

        profile.outputTranspose = static_cast<int>(profileObject->getProperty("outputTranspose"));



        profile.useChordWindow = static_cast<bool>(profileObject->getProperty("useChordWindow"));

        profile.chordWindowMs = static_cast<double>(profileObject->getProperty("chordWindowMs"));

        profile.enforceActiveVoiceLimit = static_cast<bool>(profileObject->getProperty("enforceActiveVoiceLimit"));



        profile.voiceSourceRanges.clear();



        const auto voiceRangesVar = profileObject->getProperty("voiceSourceRanges");



        if (voiceRangesVar.isArray())

        {

            const auto* voiceRangesArray = voiceRangesVar.getArray();



            if (voiceRangesArray != nullptr)

            {

                for (const auto& rangeVar : *voiceRangesArray)

                {

                    if (!rangeVar.isObject())

                        continue;



                    const auto* rangeObject = rangeVar.getDynamicObject();



                    if (rangeObject == nullptr)

                        continue;



                    VoiceSourceRange range;



                    range.rank = static_cast<int>(rangeObject->getProperty("rank"));

                    range.minNote = static_cast<int>(rangeObject->getProperty("minNote"));

                    range.maxNote = static_cast<int>(rangeObject->getProperty("maxNote"));

                    range.targetNote = static_cast<int>(rangeObject->getProperty("targetNote"));

                    range.outputTranspose = static_cast<int>(rangeObject->getProperty("outputTranspose"));



                    profile.voiceSourceRanges.push_back(range);

                }

            }

        }



        if (profile.cc31 < 0 || profile.cc31 > 127)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] cc31 out of range");
            continue;
        }

        if (profile.maxVoices < 1 || profile.maxVoices > 64)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] maxVoices out of range");
            continue;
        }

        if (profile.minNote < 0 || profile.minNote > 127
            || profile.maxNote < 0 || profile.maxNote > 127
            || profile.targetNote < 0 || profile.targetNote > 127)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] note range out of MIDI bounds");
            continue;
        }

        if (profile.minNote > profile.maxNote)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] minNote greater than maxNote");
            continue;
        }

        if (profile.chordWindowMs < 0.0 || profile.chordWindowMs > 5000.0)
        {
            ++skippedProfileCount;
            addValidationWarning("profile[" + juce::String(profileIndex) + "] chordWindowMs out of range");
            continue;
        }

        loadedProfiles.push_back(profile);

    }



    if (loadedProfiles.empty())

    {
        juce::String message = "No valid JSON profiles loaded, using built-in profiles";

        if (skippedProfileCount > 0)
            message << " | Skipped: " << skippedProfileCount;

        if (!validationWarnings.isEmpty())
            message << " | Warnings: " << validationWarnings.joinIntoString("; ");

        jsonProfileStatus = makeJsonStatus("ERROR", message);

        return;

    }



    jsonProfiles = loadedProfiles;

    jsonProfilesLoaded = true;



    juce::String successMessage = "Loaded JSON profiles successfully";

    if (skippedProfileCount > 0)
        successMessage << " | Skipped: " << skippedProfileCount;

    if (!validationWarnings.isEmpty())
        successMessage << " | Warnings: " << validationWarnings.joinIntoString("; ");

    jsonProfileStatus = makeJsonStatus("OK",
        successMessage,
        static_cast<int>(jsonProfiles.size()));

}



void DivisiCleanAudioProcessor::resetBarLookaheadState()

{

    barInputNotes.clear();

    scheduledLookaheadEvents.clear();



    activeBarStartPpq = -1.0;

    lastSeenPpq = -1.0;

    lastTransportPlaying = false;



    pendingNotes.clear();

    activeNoteMap.fill(-1);

}



bool DivisiCleanAudioProcessor::getTransportPpqInfo(double& ppqPosition,

    double& bpm,

    double& timeSigNumerator,

    double& timeSigDenominator,

    bool& isPlaying) const

{

    ppqPosition = 0.0;

    bpm = 120.0;

    timeSigNumerator = 4.0;

    timeSigDenominator = 4.0;

    isPlaying = false;



    auto* ph = getPlayHead();



    if (ph == nullptr)

        return false;



    auto position = ph->getPosition();



    if (!position)

        return false;



    auto ppq = position->getPpqPosition();



    if (ppq)

        ppqPosition = *ppq;



    auto tempo = position->getBpm();



    if (tempo)

        bpm = *tempo;



    auto sig = position->getTimeSignature();



    if (sig)

    {

        timeSigNumerator = static_cast<double>(sig->numerator);

        timeSigDenominator = static_cast<double>(sig->denominator);

    }



    isPlaying = position->getIsPlaying();



    return true;

}



double DivisiCleanAudioProcessor::getBarLengthBeats(double numerator, double denominator) const

{

    if (numerator <= 0.0 || denominator <= 0.0)

        return 4.0;



    return numerator * (4.0 / denominator);

}



double DivisiCleanAudioProcessor::getBarStartPpq(double ppqPosition, double barLengthBeats) const

{

    if (barLengthBeats <= 0.0)

        return 0.0;



    return std::floor(ppqPosition / barLengthBeats) * barLengthBeats;

}



double DivisiCleanAudioProcessor::quantizePpqToGrid(double ppq,

    double barStartPpq,

    double barLengthBeats) const

{

    const int divisions = juce::jlimit(1, 128, engineSettings.quantizeDivisionsPerBar);



    const double gridSize = barLengthBeats / static_cast<double>(divisions);

    const double local = ppq - barStartPpq;



    const double quantizedLocal = std::round(local / gridSize) * gridSize;



    return barStartPpq + juce::jlimit(0.0, barLengthBeats, quantizedLocal);

}



void DivisiCleanAudioProcessor::finaliseCompletedBar(double completedBarStartPpq,

    double barLengthBeats)

{

    std::vector<BarInputNote> completedNotes;



    for (auto note : barInputNotes)

    {

        if (note.startPpq >= completedBarStartPpq

            && note.startPpq < completedBarStartPpq + barLengthBeats)

        {

            if (!note.hasEnd)

            {

                note.endPpq = completedBarStartPpq + barLengthBeats;

                note.hasEnd = true;

            }



            completedNotes.push_back(note);

        }

    }



    barInputNotes.erase(

        std::remove_if(barInputNotes.begin(), barInputNotes.end(),

            [completedBarStartPpq, barLengthBeats](const BarInputNote& note)

            {

                return note.startPpq >= completedBarStartPpq

                    && note.startPpq < completedBarStartPpq + barLengthBeats;

            }),

        barInputNotes.end());



    if (completedNotes.empty())

        return;



    // Quantize note starts/ends first.

    for (auto& note : completedNotes)

    {

        note.startPpq = quantizePpqToGrid(note.startPpq,

            completedBarStartPpq,

            barLengthBeats);



        note.endPpq = quantizePpqToGrid(note.endPpq,

            completedBarStartPpq,

            barLengthBeats);



        if (note.endPpq <= note.startPpq)

            note.endPpq = note.startPpq + engineSettings.minNoteLengthBeats;

    }



    if (engineSettings.suppressShortNotes)

    {

        completedNotes.erase(

            std::remove_if(completedNotes.begin(), completedNotes.end(),

                [this](const BarInputNote& note)

                {

                    return (note.endPpq - note.startPpq) < engineSettings.minNoteLengthBeats;

                }),

            completedNotes.end());

    }



    if (completedNotes.empty())

        return;



    const double outputOffset = barLengthBeats * engineSettings.outputDelayBars;



    // Small musical delay added to notes after the CC31 preset switch.

    // This gives Divisimate a little time to activate the new preset before notes arrive.

    // At 120 BPM:

    //   0.01 beats ≈ 5 ms

    //   0.02 beats ≈ 10 ms

    //   0.04 beats ≈ 20 ms

    //

    // Start conservatively with 0.02. If Divisimate still switches late, try 0.04.

    const double presetLeadBeats = 0.02;



    // Group completed notes by the CC31/profile that was active when each note-on arrived.

    // This prevents later CC31 changes from reprocessing old buffered notes with the wrong profile.

    std::map<int, std::vector<BarInputNote>> notesByCC31;



    for (const auto& note : completedNotes)

        notesByCC31[note.cc31].push_back(note);



    auto scheduleCC31AtPpq = [this](int cc31, double targetPpq)

        {

            if (cc31 < 0)

                return;



            scheduledLookaheadEvents.push_back(

                {

                    juce::MidiMessage::controllerEvent(

                        1,

                        31,

                        juce::jlimit(0, 127, cc31)),

                    targetPpq

                });

        };



    auto scheduleNotePair = [this, outputOffset, presetLeadBeats](int channel,

        int outputNote,

        int velocity,

        double startPpq,

        double endPpq)

        {

            const auto noteOn = juce::MidiMessage::noteOn(channel,

                juce::jlimit(0, 127, outputNote),

                static_cast<juce::uint8>(juce::jlimit(1, 127, velocity)));



            const auto noteOff = juce::MidiMessage::noteOff(channel,

                juce::jlimit(0, 127, outputNote));



            // Notes are nudged slightly after the CC31 for their profile.

            // This gives Divisimate time to switch presets before note-ons arrive.

            scheduledLookaheadEvents.push_back(

                {

                    noteOn,

                    startPpq + outputOffset + presetLeadBeats

                });



            scheduledLookaheadEvents.push_back(

                {

                    noteOff,

                    endPpq + outputOffset + presetLeadBeats

                });

        };



    for (auto& ccPair : notesByCC31)

    {

        const int cc31ForThisGroup = ccPair.first;

        auto& notesForThisProfile = ccPair.second;



        if (notesForThisProfile.empty())

            continue;



        const auto currentProfile = getProfileForCC31(cc31ForThisGroup);



        // Emit this group's CC31 at the first note position of this group,

        // not blindly at the bar start.

        //

        // This avoids multiple different CC31 values fighting at the exact same

        // output bar-start PPQ.

        double firstNoteStartPpq = notesForThisProfile.front().startPpq;



        for (const auto& note : notesForThisProfile)

            firstNoteStartPpq = std::min(firstNoteStartPpq, note.startPpq);



        scheduleCC31AtPpq(cc31ForThisGroup, firstNoteStartPpq + outputOffset);



        if (currentProfile.type == ProfileType::PassThrough)

        {

            // Current policy:

            // In BarLookahead mode, PassThrough-profile notes are not emitted.

            //

            // This preserves the existing behavior. If needed later, this can be changed

            // so PassThrough notes are delayed and re-emitted unchanged.

            continue;

        }



        // Group notes by quantized onset within this CC31/profile group.

        std::map<double, std::vector<BarInputNote>> onsetGroups;



        for (const auto& note : notesForThisProfile)

            onsetGroups[note.startPpq].push_back(note);



        for (auto& groupPair : onsetGroups)

        {

            auto& groupNotes = groupPair.second;



            if (groupNotes.empty())

                continue;



            std::sort(groupNotes.begin(), groupNotes.end(),

                [](const BarInputNote& a, const BarInputNote& b)

                {

                    if (a.inputNote == b.inputNote)

                        return a.channel < b.channel;



                    return a.inputNote < b.inputNote;

                });



            std::vector<int> noteNumbers;

            noteNumbers.reserve(groupNotes.size());



            for (const auto& note : groupNotes)

                noteNumbers.push_back(note.inputNote);



            if (currentProfile.type == ProfileType::SingleSource)

            {

                const int selectedIndex =

                    chooseSingleSourceIndexFromNotes(noteNumbers, currentProfile);



                if (selectedIndex >= 0

                    && selectedIndex < static_cast<int>(groupNotes.size()))

                {

                    const auto& selected = groupNotes[(size_t)selectedIndex];



                    int outputNote = wrapNoteNearTarget(selected.inputNote,

                        currentProfile.minNote,

                        currentProfile.maxNote,

                        currentProfile.targetNote);



                    outputNote = applyOutputTranspose(outputNote,

                        currentProfile.outputTranspose);



                    scheduleNotePair(selected.channel,

                        outputNote,

                        selected.velocity,

                        selected.startPpq,

                        selected.endPpq);

                }



                continue;

            }



            if (currentProfile.type == ProfileType::BlockVoicing)

            {

                auto selectedIndices =

                    chooseBlockVoicingIndicesFromNotes(noteNumbers, currentProfile);



                if (currentProfile.registerWrapMode == RegisterWrapMode::PerVoiceRange)

                {

                    std::sort(selectedIndices.begin(), selectedIndices.end(),

                        [&noteNumbers](int a, int b)

                        {

                            return noteNumbers[(size_t)a] < noteNumbers[(size_t)b];

                        });

                }



                std::vector<int> usedOutputNotes;



                for (int rank = 0; rank < static_cast<int>(selectedIndices.size()); ++rank)

                {

                    const int selectedIndex = selectedIndices[(size_t)rank];



                    if (selectedIndex < 0

                        || selectedIndex >= static_cast<int>(groupNotes.size()))

                    {

                        continue;

                    }



                    const auto& selected = groupNotes[(size_t)selectedIndex];



                    VoiceSourceRange range;



                    if (currentProfile.registerWrapMode == RegisterWrapMode::PerVoiceRange)

                    {

                        range = getVoiceSourceRangeForProfileRank(currentProfile,

                            rank,

                            static_cast<int>(selectedIndices.size()));

                    }

                    else

                    {

                        range = {

                            rank,

                            currentProfile.minNote,

                            currentProfile.maxNote,

                            currentProfile.targetNote,

                            currentProfile.outputTranspose

                        };

                    }



                    int outputNote = wrapNoteNearTarget(selected.inputNote,

                        range.minNote,

                        range.maxNote,

                        range.targetNote);



                    outputNote = applyOutputTranspose(outputNote,

                        range.outputTranspose);



                    if (std::find(usedOutputNotes.begin(),

                        usedOutputNotes.end(),

                        outputNote) != usedOutputNotes.end())

                    {

                        const int up = outputNote + 12;

                        const int down = outputNote - 12;



                        if (up <= range.maxNote

                            && std::find(usedOutputNotes.begin(),

                                usedOutputNotes.end(),

                                up) == usedOutputNotes.end())

                        {

                            outputNote = up;

                        }

                        else if (down >= range.minNote

                            && std::find(usedOutputNotes.begin(),

                                usedOutputNotes.end(),

                                down) == usedOutputNotes.end())

                        {

                            outputNote = down;

                        }

                    }



                    if (std::find(usedOutputNotes.begin(),

                        usedOutputNotes.end(),

                        outputNote) != usedOutputNotes.end())

                    {

                        continue;

                    }



                    usedOutputNotes.push_back(outputNote);



                    scheduleNotePair(selected.channel,

                        outputNote,

                        selected.velocity,

                        selected.startPpq,

                        selected.endPpq);

                }

            }

        }

    }



    std::sort(scheduledLookaheadEvents.begin(), scheduledLookaheadEvents.end(),

        [](const ScheduledMidiEvent& a, const ScheduledMidiEvent& b)

        {

            if (a.targetPpq == b.targetPpq)

            {

                // Controllers first, so CC31 preset changes arrive before notes.

                if (a.message.isController() && !b.message.isController())

                    return true;



                if (!a.message.isController() && b.message.isController())

                    return false;



                // Then note-offs before note-ons, to avoid retrigger/hanging issues.

                if (a.message.isNoteOff() && b.message.isNoteOn())

                    return true;



                if (a.message.isNoteOn() && b.message.isNoteOff())

                    return false;

            }



            return a.targetPpq < b.targetPpq;

        });

}



void DivisiCleanAudioProcessor::emitScheduledEventsForBlock(juce::MidiBuffer& outputMidi,

    double blockStartPpq,

    double blockEndPpq,

    int numSamples)

{

    if (numSamples <= 0)

        return;



    if (blockEndPpq <= blockStartPpq)

        return;



    constexpr double ppqEpsilon = 0.0001;



    std::vector<ScheduledMidiEvent> remainingEvents;

    remainingEvents.reserve(scheduledLookaheadEvents.size());



    for (const auto& event : scheduledLookaheadEvents)

    {

        // Future event: keep it.

        if (event.targetPpq > blockEndPpq + ppqEpsilon)

        {

            remainingEvents.push_back(event);

            continue;

        }



        int samplePosition = 0;



        // Overdue or exactly at block start: emit immediately.

        if (event.targetPpq <= blockStartPpq + ppqEpsilon)

        {

            samplePosition = 0;

        }

        else

        {

            const double ratio =

                (event.targetPpq - blockStartPpq) / (blockEndPpq - blockStartPpq);



            samplePosition = juce::jlimit(0,

                juce::jmax(0, numSamples - 1),

                juce::roundToInt(ratio * static_cast<double>(numSamples)));

        }



        outputMidi.addEvent(event.message, samplePosition);

    }



    scheduledLookaheadEvents.swap(remainingEvents);

}



void DivisiCleanAudioProcessor::processBarLookaheadBlock(juce::AudioBuffer<float>& buffer,

    juce::MidiBuffer& midiMessages)

{

    buffer.clear();



    juce::MidiBuffer outputMidi;



    if (midiPanicAndResetRequested.exchange(false))

    {

        addMidiPanicMessages(outputMidi, 0);

        resetBarLookaheadState();

        midiMessages.swapWith(outputMidi);

        return;

    }



    double ppq = 0.0;

    double bpm = 120.0;

    double numerator = 4.0;

    double denominator = 4.0;

    bool isPlaying = false;



    const bool hasTransport = getTransportPpqInfo(ppq, bpm, numerator, denominator, isPlaying);



    if (!hasTransport || !isPlaying)

    {

        if (engineSettings.resetOnTransportStop && lastTransportPlaying)

        {

            addMidiPanicMessages(outputMidi, 0);

            resetBarLookaheadState();

        }



        lastTransportPlaying = isPlaying;



        // While stopped, pass MIDI through so manual/controller input still works.

        // CC31 still updates DivisiClean's internal GUI/profile state.

        //

        // Important:

        // When stopped, there is no BarLookahead timeline to align against,

        // so passing CC31 through immediately is acceptable and useful.

        for (const auto metadata : midiMessages)

        {

            const auto message = metadata.getMessage();



            if (message.isController() && message.getControllerNumber() == 31)

                activeCC31.store(message.getControllerValue());



            outputMidi.addEvent(message, metadata.samplePosition);

        }



        midiMessages.swapWith(outputMidi);

        return;

    }



    const double barLengthBeats = getBarLengthBeats(numerator, denominator);

    const double blockStartPpq = ppq;



    const double beatsPerSecond = bpm / 60.0;

    const double blockDurationSeconds =

        static_cast<double>(buffer.getNumSamples()) / getSampleRate();



    const double blockLengthBeats = beatsPerSecond * blockDurationSeconds;

    const double blockEndPpq = blockStartPpq + blockLengthBeats;



    const double currentBarStart = getBarStartPpq(blockStartPpq, barLengthBeats);



    if (activeBarStartPpq < 0.0)

        activeBarStartPpq = currentBarStart;



    const bool jumpedBack =

        lastSeenPpq >= 0.0

        && blockStartPpq + 0.0001 < lastSeenPpq;



    if (jumpedBack && engineSettings.resetOnLoopJump)

    {

        addMidiPanicMessages(outputMidi, 0);

        resetBarLookaheadState();

        activeBarStartPpq = currentBarStart;

    }



    while (activeBarStartPpq + barLengthBeats <= currentBarStart)

    {

        finaliseCompletedBar(activeBarStartPpq, barLengthBeats);

        activeBarStartPpq += barLengthBeats;

    }



    emitScheduledEventsForBlock(outputMidi,

        blockStartPpq,

        blockEndPpq,

        buffer.getNumSamples());



    for (const auto metadata : midiMessages)

    {

        const auto message = metadata.getMessage();



        const double eventRatio =

            buffer.getNumSamples() > 0

            ? static_cast<double>(metadata.samplePosition) / static_cast<double>(buffer.getNumSamples())

            : 0.0;



        const double eventPpq =

            blockStartPpq + (eventRatio * blockLengthBeats);



        if (message.isController())

        {

            if (message.getControllerNumber() == 31)

            {

                // In BarLookahead mode, CC31 is part of the delayed musical timeline.

                //

                // Do not output it immediately.

                // Do not panic.

                // Do not clear scheduled events.

                //

                // Instead, update DivisiClean's internal active profile now.

                // New note-ons captured after this point will be tagged with this CC31.

                // The matching CC31 will be emitted later by finaliseCompletedBar(),

                // at the same delayed output bar as the notes that belong to it.

                activeCC31.store(message.getControllerValue());



                continue;

            }



            // Other controllers can still pass through immediately.

            outputMidi.addEvent(message, metadata.samplePosition);

            continue;

        }



        if (message.isNoteOn())

        {

            barInputNotes.push_back(

                {

                    message.getChannel(),

                    message.getNoteNumber(),

                    static_cast<int>(message.getVelocity()),

                    eventPpq,

                    eventPpq,

                    false,

                    activeCC31.load()

                });



            continue;

        }



        if (message.isNoteOff())

        {

            for (auto it = barInputNotes.rbegin(); it != barInputNotes.rend(); ++it)

            {

                if (it->channel == message.getChannel()

                    && it->inputNote == message.getNoteNumber()

                    && !it->hasEnd)

                {

                    it->endPpq = eventPpq;

                    it->hasEnd = true;

                    break;

                }

            }



            continue;

        }



        // Non-note MIDI passes through.

        outputMidi.addEvent(message, metadata.samplePosition);

    }



    lastSeenPpq = blockStartPpq;

    lastTransportPlaying = isPlaying;



    midiMessages.swapWith(outputMidi);

}



void DivisiCleanAudioProcessor::reloadJsonProfilesFromGui()

{

    // JSON loading remains on the GUI/message thread.

    // MIDI state reset and panic are requested for the next processBlock(),

    // so note maps are not cleared directly from the GUI thread.

    loadJsonProfiles();



    requestMidiPanicAndStateReset();



    ++jsonReloadCount;



    jsonProfileStatus += " | reload "

        + juce::String(jsonReloadCount)

        + " | panic/reset requested";

}



void DivisiCleanAudioProcessor::requestMidiPanicAndStateReset()

{

    midiPanicAndResetRequested.store(true);

}



void DivisiCleanAudioProcessor::addMidiPanicMessages(juce::MidiBuffer& outputMidi, int samplePosition) const

{

    for (int channel = 1; channel <= 16; ++channel)

    {

        // Sustain pedal off.

        outputMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), samplePosition);



        // All Sound Off.

        outputMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), samplePosition);



        // Reset All Controllers.

        outputMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 121, 0), samplePosition);



        // All Notes Off.

        outputMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), samplePosition);



        // Explicit note-offs as a belt-and-braces fallback for hosts/instruments

        // that ignore All Notes Off or All Sound Off.

        for (int note = 0; note < 128; ++note)

            outputMidi.addEvent(juce::MidiMessage::noteOff(channel, note), samplePosition);

    }

}



juce::String DivisiCleanAudioProcessor::getJsonProfileStatus() const

{

    return jsonProfileStatus;

}



//==============================================================================

const juce::String DivisiCleanAudioProcessor::getName() const

{

    return JucePlugin_Name;

}



bool DivisiCleanAudioProcessor::acceptsMidi() const

{

   #if JucePlugin_WantsMidiInput

    return true;

   #else

    return false;

   #endif

}



bool DivisiCleanAudioProcessor::producesMidi() const

{

   #if JucePlugin_ProducesMidiOutput

    return true;

   #else

    return false;

   #endif

}



bool DivisiCleanAudioProcessor::isMidiEffect() const

{

   #if JucePlugin_IsMidiEffect

    return true;

   #else

    return false;

   #endif

}



double DivisiCleanAudioProcessor::getTailLengthSeconds() const

{

    return 0.0;

}



int DivisiCleanAudioProcessor::getNumPrograms()

{

    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,

                // so this should be at least 1, even if you're not really implementing programs.

}



int DivisiCleanAudioProcessor::getCurrentProgram()

{

    return 0;

}



void DivisiCleanAudioProcessor::setCurrentProgram (int index)

{

}



const juce::String DivisiCleanAudioProcessor::getProgramName (int index)

{

    return {};

}



void DivisiCleanAudioProcessor::changeProgramName (int index, const juce::String& newName)

{

}



//==============================================================================

void DivisiCleanAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)

{

    juce::ignoreUnused(sampleRate, samplesPerBlock);



    pendingNotes.clear();

    activeNoteMap.fill(-1);

}



void DivisiCleanAudioProcessor::releaseResources()

{

    resetBarLookaheadState();

}



#ifndef JucePlugin_PreferredChannelConfigurations

bool DivisiCleanAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const

{

  #if JucePlugin_IsMidiEffect

    juce::ignoreUnused (layouts);

    return true;

  #else

    // This is the place where you check if the layout is supported.

    // In this template code we only support mono or stereo.

    // Some plugin hosts, such as certain GarageBand versions, will only

    // load plugins that support stereo bus layouts.

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()

     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())

        return false;



    // This checks if the input layout matches the output layout

   #if ! JucePlugin_IsSynth

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())

        return false;

   #endif



    return true;

  #endif

}

#endif



juce::String DivisiCleanAudioProcessor::getBarLookaheadDebugText() const

{

    if (scheduledLookaheadEvents.empty())

        return "Sched PPQ: none";



    double minPpq = scheduledLookaheadEvents.front().targetPpq;

    double maxPpq = scheduledLookaheadEvents.front().targetPpq;



    for (const auto& event : scheduledLookaheadEvents)

    {

        minPpq = std::min(minPpq, event.targetPpq);

        maxPpq = std::max(maxPpq, event.targetPpq);

    }



    return "Sched PPQ: "

        + juce::String(minPpq, 3)

        + " - "

        + juce::String(maxPpq, 3)

        + " | Last PPQ: "

        + juce::String(lastSeenPpq, 3);

}



juce::String DivisiCleanAudioProcessor::engineModeToStorageString(EngineMode mode)

{

    switch (mode)

    {

    case EngineMode::BarLookahead:

        return "barLookahead";



    case EngineMode::GeneratedMidi:

        return "generatedMidi";



    case EngineMode::ArpDriver:

        return "arpDriver";



    case EngineMode::ChordWindow:

    default:

        return "chordWindow";

    }

}



EngineMode DivisiCleanAudioProcessor::engineModeFromStorageString(const juce::String& text)

{

    const auto t = normaliseEnumText(text);



    if (t == "barlookahead")

        return EngineMode::BarLookahead;



    if (t == "generatedmidi")

        return EngineMode::GeneratedMidi;



    if (t == "arpdriver")

        return EngineMode::ArpDriver;



    return EngineMode::ChordWindow;

}



void DivisiCleanAudioProcessor::setEngineMode(EngineMode newMode)

{

    selectedEngineMode.store(static_cast<int>(newMode));



    switch (newMode)

    {

    case EngineMode::BarLookahead:

        engineSettings.timingMode = TimingMode::BarLookahead;

        break;



    case EngineMode::ChordWindow:

    case EngineMode::GeneratedMidi:

    case EngineMode::ArpDriver:

    default:

        engineSettings.timingMode = TimingMode::ChordWindow;

        break;

    }

}



EngineMode DivisiCleanAudioProcessor::getEngineMode() const

{

    const int mode = selectedEngineMode.load();



    switch (mode)

    {

    case static_cast<int>(EngineMode::BarLookahead):

        return EngineMode::BarLookahead;



    case static_cast<int>(EngineMode::GeneratedMidi):

        return EngineMode::GeneratedMidi;



    case static_cast<int>(EngineMode::ArpDriver):

        return EngineMode::ArpDriver;



    case static_cast<int>(EngineMode::ChordWindow):

    default:

        return EngineMode::ChordWindow;

    }

}



juce::String DivisiCleanAudioProcessor::getEngineModeDisplayName() const

{

    switch (getEngineMode())

    {

    case EngineMode::BarLookahead:

        return "Bar Lookahead";



    case EngineMode::GeneratedMidi:

        return "Generated MIDI";



    case EngineMode::ArpDriver:

        return "ARP Driver";



    case EngineMode::ChordWindow:

    default:

        return "Chord Window";

    }

}



juce::String DivisiCleanAudioProcessor::getEngineTimingModeName() const

{

    switch (engineSettings.timingMode)

    {

    case TimingMode::BarLookahead:

        return "BarLookahead";



    case TimingMode::ChordWindow:

    default:

        return "ChordWindow";

    }

}



juce::String DivisiCleanAudioProcessor::getEngineStatusText() const

{

    if (engineSettings.enabled

        && engineSettings.timingMode == TimingMode::BarLookahead)

    {

        return "Engine: BarLookahead | Grid: 1/"

            + juce::String(engineSettings.quantizeDivisionsPerBar)

            + " bar | Delay: "

            + juce::String(engineSettings.outputDelayBars, 1)

            + " bar";

    }



    return "Engine: ChordWindow";

}



int DivisiCleanAudioProcessor::getBarLookaheadBufferedNoteCount() const

{

    return static_cast<int>(barInputNotes.size());

}



int DivisiCleanAudioProcessor::getBarLookaheadScheduledEventCount() const

{

    return static_cast<int>(scheduledLookaheadEvents.size());

}



void DivisiCleanAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,

    juce::MidiBuffer& midiMessages)

{

    juce::ScopedNoDenormals noDenormals;



    // This plugin produces no audio.

    buffer.clear();



    if (engineSettings.enabled

        && engineSettings.timingMode == TimingMode::BarLookahead)

    {

        processBarLookaheadBlock(buffer, midiMessages);

        return;

    }



    juce::MidiBuffer processedBuffer;



    if (midiPanicAndResetRequested.exchange(false))

    {

        addMidiPanicMessages(processedBuffer, 0);



        pendingNotes.clear();

        activeNoteMap.fill(-1);

        resetBarLookaheadState();

    }



    const double blockMs = samplesToMs(buffer.getNumSamples());



    for (auto& pending : pendingNotes)

        pending.ageMs += blockMs;



    // Important:

    // Flush pending notes that became ready before reading new MIDI events

    // from this block. Otherwise, new rhythmic material can be merged into

    // an older chord window.

    flushPendingNotesIfReady(processedBuffer);



    const auto currentProfile = getProfileForCC31(activeCC31.load());



    // Pass-through profiles:

    // Detect CC31, otherwise leave midiMessages unchanged.

    if (currentProfile.type == ProfileType::PassThrough)

    {

        // If we flushed anything above, we cannot simply return with the

        // original midiMessages unchanged. We need to preserve input events too.

        for (const auto metadata : midiMessages)

        {

            const auto message = metadata.getMessage();

            const int samplePosition = metadata.samplePosition;



            if (message.isController() && message.getControllerNumber() == 31)

                activeCC31.store(message.getControllerValue());



            processedBuffer.addEvent(message, samplePosition);

        }



        midiMessages.swapWith(processedBuffer);

        return;

    }



    struct PendingNoteOn

    {

        juce::MidiMessage message;

        int samplePosition = 0;

    };



    std::vector<PendingNoteOn> pendingNoteOns;



    // First pass:

    // - pass through controllers and non-note events

    // - collect note-ons

    // - transform mapped note-offs

    for (const auto metadata : midiMessages)

    {

        const auto message = metadata.getMessage();

        const int samplePosition = metadata.samplePosition;



        if (message.isController())

        {

            if (message.getControllerNumber() == 31)

            {

                // If the profile changes, flush any delayed notes first.

                flushPendingNotesNow(processedBuffer);



                activeCC31.store(message.getControllerValue());

            }



            processedBuffer.addEvent(message, samplePosition);

            continue;

        }



        if (message.isNoteOn())

        {

            const auto profileNow = getProfileForCC31(activeCC31.load());



            if (profileNow.useChordWindow

                && (profileNow.type == ProfileType::BlockVoicing

                    || profileNow.type == ProfileType::SingleSource))

            {

                pendingNotes.push_back(

                    PendingNote

                    {

                        message.getChannel(),

                        message.getNoteNumber(),

                        static_cast<int>(message.getVelocity()),

                        0.0

                    }

                );

            }

            else

            {

                pendingNoteOns.push_back({ message, samplePosition });

            }



            continue;

        }



        if (message.isNoteOff())

        {

            bool cancelledPendingNote = false;



            // If this note-on is still waiting inside the chord window,

            // cancel it and do not send any note-off.

            for (auto it = pendingNotes.begin(); it != pendingNotes.end(); ++it)

            {

                if (it->channel == message.getChannel()

                    && it->noteNumber == message.getNoteNumber())

                {

                    pendingNotes.erase(it);

                    cancelledPendingNote = true;

                    break;

                }

            }



            if (cancelledPendingNote)

                continue;



            const int channel = message.getChannel();

            const int inputNote = message.getNoteNumber();

            const int mapIndex = getNoteMapIndex(channel, inputNote);

            const int mappedOutputNote = activeNoteMap[(size_t)mapIndex];



            if (mappedOutputNote >= 0)

            {

                auto noteOff = juce::MidiMessage::noteOff(channel, mappedOutputNote);

                processedBuffer.addEvent(noteOff, samplePosition);

                activeNoteMap[(size_t)mapIndex] = -1;

            }

            else if (mappedOutputNote == -2)

            {

                // This input note was intentionally suppressed earlier.

                // Suppress its note-off too.

                activeNoteMap[(size_t)mapIndex] = -1;

            }

            else

            {

                // No mapping exists. In transformed modes, suppress unknown note-offs.

            }



            continue;

        }



        // Pass through pitch bend, aftertouch, program changes, etc.

        processedBuffer.addEvent(message, samplePosition);

    }



    // Normal immediate processing for profiles that do not use the chord window.

    if (!pendingNoteOns.empty())

    {

        std::vector<int> noteNumbers;

        noteNumbers.reserve(pendingNoteOns.size());



        for (const auto& pending : pendingNoteOns)

            noteNumbers.push_back(pending.message.getNoteNumber());



        if (currentProfile.type == ProfileType::SingleSource)

        {

            const int selectedIndex = chooseSingleSourceIndexFromNotes(noteNumbers, currentProfile);



            const auto& selected = pendingNoteOns[(size_t)selectedIndex];



            const int inputChannel = selected.message.getChannel();

            const int inputNote = selected.message.getNoteNumber();

            const float velocity = selected.message.getFloatVelocity();



            int outputNote = wrapNoteNearTarget(inputNote,

                currentProfile.minNote,

                currentProfile.maxNote,

                currentProfile.targetNote);



            outputNote = applyOutputTranspose(outputNote, currentProfile.outputTranspose);



            auto transformedNoteOn = juce::MidiMessage::noteOn(inputChannel, outputNote, velocity);

            processedBuffer.addEvent(transformedNoteOn, selected.samplePosition);



            for (int i = 0; i < static_cast<int>(pendingNoteOns.size()); ++i)

            {

                const auto& pending = pendingNoteOns[(size_t)i];



                const int channel = pending.message.getChannel();

                const int input = pending.message.getNoteNumber();

                const int mapIndex = getNoteMapIndex(channel, input);



                if (i == selectedIndex)

                    activeNoteMap[(size_t)mapIndex] = outputNote;

                else

                    activeNoteMap[(size_t)mapIndex] = -2;

            }

        }

        else if (currentProfile.type == ProfileType::BlockVoicing)

        {

            const auto selectedIndices = chooseBlockVoicingIndicesFromNotes(noteNumbers, currentProfile);



            std::vector<int> usedOutputNotes;



            auto getActiveMappedVoiceCount = [this]()

                {

                    int count = 0;



                    for (const auto mappedNote : activeNoteMap)

                    {

                        if (mappedNote >= 0)

                            ++count;

                    }



                    return count;

                };



            // First mark every pending note as suppressed.

            // Selected/emitted notes will overwrite this with their actual output mapping.

            for (const auto& pending : pendingNoteOns)

            {

                const int channel = pending.message.getChannel();

                const int inputNote = pending.message.getNoteNumber();

                const int mapIndex = getNoteMapIndex(channel, inputNote);



                activeNoteMap[(size_t)mapIndex] = -2;

            }



            for (const int selectedIndex : selectedIndices)

            {

                const auto& selected = pendingNoteOns[(size_t)selectedIndex];



                const int inputChannel = selected.message.getChannel();

                const int inputNote = selected.message.getNoteNumber();

                const float velocity = selected.message.getFloatVelocity();



                if (currentProfile.enforceActiveVoiceLimit

                    && getActiveMappedVoiceCount() >= currentProfile.maxVoices)

                {

                    // Leave this note marked as suppressed.

                    continue;

                }



                int outputNote = wrapNoteNearTarget(inputNote,

                    currentProfile.minNote,

                    currentProfile.maxNote,

                    currentProfile.targetNote);



                outputNote = applyOutputTranspose(outputNote, currentProfile.outputTranspose);



                // Avoid duplicate output notes if two input notes wrap to the same pitch.

                // Try moving one octave up, then one octave down.

                if (std::find(usedOutputNotes.begin(), usedOutputNotes.end(), outputNote) != usedOutputNotes.end())

                {

                    const int up = outputNote + 12;

                    const int down = outputNote - 12;



                    if (up <= currentProfile.maxNote

                        && std::find(usedOutputNotes.begin(), usedOutputNotes.end(), up) == usedOutputNotes.end())

                    {

                        outputNote = up;

                    }

                    else if (down >= currentProfile.minNote

                        && std::find(usedOutputNotes.begin(), usedOutputNotes.end(), down) == usedOutputNotes.end())

                    {

                        outputNote = down;

                    }

                }



                // If still duplicate, suppress this note to prevent note-off confusion.

                if (std::find(usedOutputNotes.begin(), usedOutputNotes.end(), outputNote) != usedOutputNotes.end())

                    continue;



                usedOutputNotes.push_back(outputNote);



                auto transformedNoteOn = juce::MidiMessage::noteOn(inputChannel, outputNote, velocity);

                processedBuffer.addEvent(transformedNoteOn, selected.samplePosition);



                const int mapIndex = getNoteMapIndex(inputChannel, inputNote);

                activeNoteMap[(size_t)mapIndex] = outputNote;

            }

        }

    }



    flushPendingNotesIfReady(processedBuffer);



    midiMessages.swapWith(processedBuffer);

}



int DivisiCleanAudioProcessor::applyOutputTranspose(int noteNumber, int transposeSemitones) const

{

    return juce::jlimit(0, 127, noteNumber + transposeSemitones);

}



int DivisiCleanAudioProcessor::getActiveCC31() const

{

    return activeCC31.load();

}



juce::String DivisiCleanAudioProcessor::getActiveProfileName() const

{

    return getProfileNameForCC31(activeCC31.load());

}



juce::String DivisiCleanAudioProcessor::getActiveProfileTypeName() const

{

    const auto profile = getProfileForCC31(activeCC31.load());



    switch (profile.type)

    {

    case ProfileType::PassThrough:

        return "PassThrough";



    case ProfileType::SingleSource:

        return "SingleSource";



    case ProfileType::SectionPoly:

        return "SectionPoly";



    case ProfileType::BlockVoicing:

        return "BlockVoicing";



    default:

        return "Unknown";

    }

}



juce::String DivisiCleanAudioProcessor::getActiveSourceReductionModeName() const

{

    const auto profile = getProfileForCC31(activeCC31.load());



    switch (profile.reductionMode)

    {

    case SourceReductionMode::None:

        return "None";



    case SourceReductionMode::LowestN:

        return "Lowest N";



    case SourceReductionMode::HighestN:

        return "Highest N";



    case SourceReductionMode::Spread:

        return "Spread";



    case SourceReductionMode::ClosestToTarget:

        return "Closest To Target";



    case SourceReductionMode::AsPlayed:

        return "As Played";



    default:

        return "Unknown";

    }

}



juce::String DivisiCleanAudioProcessor::getActiveExpectedDivisiModeName() const

{

    const auto profile = getProfileForCC31(activeCC31.load());



    switch (profile.expectedDivisiMode)

    {

    case ExpectedDivisiMode::None:

        return "None";



    case ExpectedDivisiMode::BottomUp:

        return "Bottom Up";



    case ExpectedDivisiMode::TopDown:

        return "Top Down";



    case ExpectedDivisiMode::FillVoices:

        return "Fill Voices";



    default:

        return "Unknown";

    }

}



juce::String DivisiCleanAudioProcessor::registerWrapModeToString(RegisterWrapMode mode) const

{

    switch (mode)

    {

    case RegisterWrapMode::PerNoteNearTarget:

        return "PerNoteNearTarget";



    case RegisterWrapMode::PerVoiceRange:

        return "PerVoiceRange";



    default:

        return "Unknown";

    }

}



juce::String DivisiCleanAudioProcessor::getActiveRegisterWrapModeName() const

{

    const auto profile = getProfileForCC31(activeCC31.load());

    return registerWrapModeToString(profile.registerWrapMode);

}



VoiceSourceRange DivisiCleanAudioProcessor::getVoiceSourceRangeForProfileRank(

    const PresetProfile& profile,

    int rank,

    int totalRanks) const

{

    juce::ignoreUnused(totalRanks);



    for (const auto& range : profile.voiceSourceRanges)

    {

        if (range.rank == rank)

            return range;

    }



    return {

    rank,

    profile.minNote,

    profile.maxNote,

    profile.targetNote,

    profile.outputTranspose

    };

}



int DivisiCleanAudioProcessor::getActiveProfileMaxVoices() const

{

    const auto profile = getProfileForCC31(activeCC31.load());

    return profile.maxVoices;

}



double DivisiCleanAudioProcessor::getActiveProfileChordWindowMs() const

{

    const auto profile = getProfileForCC31(activeCC31.load());

    return profile.chordWindowMs;

}



bool DivisiCleanAudioProcessor::getActiveProfileUsesChordWindow() const

{

    const auto profile = getProfileForCC31(activeCC31.load());

    return profile.useChordWindow;

}



bool DivisiCleanAudioProcessor::getActiveProfileEnforcesActiveVoiceLimit() const

{

    const auto profile = getProfileForCC31(activeCC31.load());

    return profile.enforceActiveVoiceLimit;

}



int DivisiCleanAudioProcessor::getPendingNoteCount() const

{

    return static_cast<int>(pendingNotes.size());

}



PresetProfile DivisiCleanAudioProcessor::getProfileForCC31(int cc31) const

{

    if (jsonProfilesLoaded)

    {

        for (const auto& profile : jsonProfiles)

        {

            if (profile.cc31 == cc31)

                return profile;

        }

    }



    return getHardcodedProfileForCC31(cc31);

}



PresetProfile DivisiCleanAudioProcessor::getHardcodedProfileForCC31(int cc31) const

{

    static const PresetProfile profiles[] =

    {

        {

            70,

            "Vln1 + Vln2 + Vc 8va",

            ProfileType::SingleSource,

            SourceSelectionMode::Highest,

            SourceReductionMode::None,

            ExpectedDivisiMode::TopDown,

            RegisterWrapMode::PerNoteNearTarget,

            1,

            48,

            84,

            72,

            0,

            true,

            250.0,

            true,

            {}

        },



        {

            80,

            "Strings open 02",

            ProfileType::BlockVoicing,

            SourceSelectionMode::Highest,

            SourceReductionMode::Spread,

            ExpectedDivisiMode::BottomUp,

            RegisterWrapMode::PerVoiceRange,

            3,

            48,

            84,

            60,

            0,

            true,

            250.0,

            true,

            {

                { 0, 40, 60, 48, 0 },

                { 1, 55, 67, 60, 0 },

                { 2, 60, 79, 67, 0 }

            }

        },



        {

            88,

            "Tutti Bass Unison",

            ProfileType::SingleSource,

            SourceSelectionMode::Highest,

            SourceReductionMode::None,

            ExpectedDivisiMode::BottomUp,

            RegisterWrapMode::PerNoteNearTarget,

            1,

            36,

            57,

            43,

            0,

            true,

            250.0,

            true,

            {}

        }

    };



    for (const auto& profile : profiles)

    {

        if (profile.cc31 == cc31)

            return profile;

    }



    return {

        cc31,

        "Unknown / Pass-through",

        ProfileType::PassThrough,

        SourceSelectionMode::Highest,

        SourceReductionMode::None,

        ExpectedDivisiMode::None,

        RegisterWrapMode::PerNoteNearTarget,

        16,

        0,

        127,

        60,

        0,

        false,

        0.0,

        false,

        {}

    };

}



juce::String DivisiCleanAudioProcessor::getProfileNameForCC31(int cc31) const

{

    if (cc31 < 0)

        return "No CC31 received";



    const auto profile = getProfileForCC31(cc31);



    if (profile.type != ProfileType::PassThrough)

        return profile.name;



    switch (cc31)

    {

    case 65: return "Violins 1";

    case 66: return "Violins 2";

    case 67: return "Violas";

    case 68: return "Cellos";



    case 73: return "Strings quintet";

    case 77: return "Strings quartet 01";

    case 78: return "Strings quartet 02";

    case 79: return "Strings open 01";



    default:

        return "Unknown / Pass-through";

    }

}



int DivisiCleanAudioProcessor::getNoteMapIndex(int channel, int noteNumber) const

{

    const int safeChannel = juce::jlimit(1, 16, channel);

    const int safeNote = juce::jlimit(0, 127, noteNumber);



    return ((safeChannel - 1) * 128) + safeNote;

}



double DivisiCleanAudioProcessor::samplesToMs(int numSamples) const

{

    const double sr = getSampleRate();



    if (sr <= 0.0)

        return 0.0;



    return 1000.0 * static_cast<double>(numSamples) / sr;

}



void DivisiCleanAudioProcessor::flushPendingNotesIfReady(juce::MidiBuffer& outputMidi)

{

    const auto currentProfile = getProfileForCC31(activeCC31.load());



    if (!currentProfile.useChordWindow)

        return;



    if (pendingNotes.empty())

        return;



    double oldestAgeMs = 0.0;



    for (const auto& pending : pendingNotes)

        oldestAgeMs = std::max(oldestAgeMs, pending.ageMs);



    if (oldestAgeMs >= currentProfile.chordWindowMs)

        flushPendingNotesNow(outputMidi);

}



void DivisiCleanAudioProcessor::flushPendingNotesNow(juce::MidiBuffer& outputMidi)

{

    const auto currentProfile = getProfileForCC31(activeCC31.load());



    if (pendingNotes.empty())

        return;



    if (currentProfile.type != ProfileType::BlockVoicing

        && currentProfile.type != ProfileType::SingleSource)

    {

        pendingNotes.clear();

        return;

    }



    struct PendingNoteOn

    {

        juce::MidiMessage message;

        int samplePosition = 0;

    };



    std::vector<PendingNoteOn> pendingNoteOns;

    pendingNoteOns.reserve(pendingNotes.size());



    for (const auto& pending : pendingNotes)

    {

        auto noteOn = juce::MidiMessage::noteOn(

            pending.channel,

            pending.noteNumber,

            static_cast<juce::uint8>(juce::jlimit(1, 127, pending.velocity))

        );



        pendingNoteOns.push_back({ noteOn, 0 });

    }



    pendingNotes.clear();



    std::vector<int> noteNumbers;

    noteNumbers.reserve(pendingNoteOns.size());



    for (const auto& pending : pendingNoteOns)

        noteNumbers.push_back(pending.message.getNoteNumber());



    auto getActiveMappedVoiceCount = [this]()

        {

            int count = 0;



            for (const auto mappedNote : activeNoteMap)

            {

                if (mappedNote >= 0)

                    ++count;

            }



            return count;

        };



    // Mark all pending notes as suppressed first.

    // Emitted notes will overwrite this with their real output mapping.

    for (const auto& pending : pendingNoteOns)

    {

        const int channel = pending.message.getChannel();

        const int inputNote = pending.message.getNoteNumber();

        const int mapIndex = getNoteMapIndex(channel, inputNote);



        activeNoteMap[(size_t)mapIndex] = -2;

    }



    if (currentProfile.type == ProfileType::SingleSource)

    {

        const int selectedIndex = chooseSingleSourceIndexFromNotes(noteNumbers, currentProfile);



        const auto& selected = pendingNoteOns[(size_t)selectedIndex];



        const int inputChannel = selected.message.getChannel();

        const int inputNote = selected.message.getNoteNumber();

        const float velocity = selected.message.getFloatVelocity();



        if (currentProfile.enforceActiveVoiceLimit

            && getActiveMappedVoiceCount() >= currentProfile.maxVoices)

        {

            // Leave selected note marked as suppressed.

            return;

        }



        int outputNote = wrapNoteNearTarget(inputNote,

            currentProfile.minNote,

            currentProfile.maxNote,

            currentProfile.targetNote);



        outputNote = applyOutputTranspose(outputNote, currentProfile.outputTranspose);



        auto transformedNoteOn = juce::MidiMessage::noteOn(inputChannel, outputNote, velocity);

        outputMidi.addEvent(transformedNoteOn, 0);



        const int mapIndex = getNoteMapIndex(inputChannel, inputNote);

        activeNoteMap[(size_t)mapIndex] = outputNote;



        return;

    }



    if (currentProfile.type == ProfileType::BlockVoicing)

    {

        auto selectedIndices = chooseBlockVoicingIndicesFromNotes(noteNumbers, currentProfile);



        struct PreparedSelectedNote

        {

            int selectedIndex = 0;

            int inputChannel = 1;

            int inputNote = 0;

            float velocity = 1.0f;

            int rank = 0;

            int outputNote = 0;

            int minNote = 0;

            int maxNote = 127;

        };



        std::vector<PreparedSelectedNote> preparedNotes;

        preparedNotes.reserve(selectedIndices.size());



        if (currentProfile.registerWrapMode == RegisterWrapMode::PerVoiceRange)

        {

            std::sort(selectedIndices.begin(), selectedIndices.end(),

                [&noteNumbers](int a, int b)

                {

                    return noteNumbers[(size_t)a] < noteNumbers[(size_t)b];

                });

        }



        const int totalRanks = static_cast<int>(selectedIndices.size());



        for (int rank = 0; rank < totalRanks; ++rank)

        {

            const int selectedIndex = selectedIndices[(size_t)rank];

            const auto& selected = pendingNoteOns[(size_t)selectedIndex];



            const int inputChannel = selected.message.getChannel();

            const int inputNote = selected.message.getNoteNumber();

            const float velocity = selected.message.getFloatVelocity();



            VoiceSourceRange range;



            if (currentProfile.registerWrapMode == RegisterWrapMode::PerVoiceRange)

            {

                range = getVoiceSourceRangeForProfileRank(currentProfile, rank, totalRanks);

            }

            else

            {

                range = {

                    rank,

                    currentProfile.minNote,

                    currentProfile.maxNote,

                    currentProfile.targetNote,

                    currentProfile.outputTranspose

                };

            }



            int outputNote = wrapNoteNearTarget(inputNote,

                range.minNote,

                range.maxNote,

                range.targetNote);



            outputNote = applyOutputTranspose(outputNote, range.outputTranspose);



            preparedNotes.push_back(

                {

                    selectedIndex,

                    inputChannel,

                    inputNote,

                    velocity,

                    rank,

                    outputNote,

                    range.minNote,

                    range.maxNote

                }

            );

        }



        if (currentProfile.registerWrapMode == RegisterWrapMode::PerVoiceRange)

        {

            std::sort(preparedNotes.begin(), preparedNotes.end(),

                [](const PreparedSelectedNote& a, const PreparedSelectedNote& b)

                {

                    return a.outputNote < b.outputNote;

                });

        }



        std::vector<int> usedOutputNotes;



        for (auto& prepared : preparedNotes)

        {

            if (currentProfile.enforceActiveVoiceLimit

                && getActiveMappedVoiceCount() >= currentProfile.maxVoices)

            {

                // Leave this note marked as suppressed.

                continue;

            }



            int outputNote = prepared.outputNote;



            // Avoid duplicate output notes if two input notes wrap to the same pitch.

            // Try moving one octave up, then one octave down.

            if (std::find(usedOutputNotes.begin(), usedOutputNotes.end(), outputNote) != usedOutputNotes.end())

            {

                const int up = outputNote + 12;

                const int down = outputNote - 12;



                if (up <= prepared.maxNote

                    && std::find(usedOutputNotes.begin(), usedOutputNotes.end(), up) == usedOutputNotes.end())

                {

                    outputNote = up;

                }

                else if (down >= prepared.minNote

                    && std::find(usedOutputNotes.begin(), usedOutputNotes.end(), down) == usedOutputNotes.end())

                {

                    outputNote = down;

                }

            }



            // If still duplicate, suppress this note to prevent note-off confusion.

            if (std::find(usedOutputNotes.begin(), usedOutputNotes.end(), outputNote) != usedOutputNotes.end())

                continue;



            usedOutputNotes.push_back(outputNote);



            auto transformedNoteOn = juce::MidiMessage::noteOn(prepared.inputChannel,

                outputNote,

                prepared.velocity);



            outputMidi.addEvent(transformedNoteOn, 0);



            const int mapIndex = getNoteMapIndex(prepared.inputChannel, prepared.inputNote);

            activeNoteMap[(size_t)mapIndex] = outputNote;

        }

    }

}



int DivisiCleanAudioProcessor::wrapNoteNearTarget(int inputNote,

    int minNote,

    int maxNote,

    int targetNote) const

{

    int bestNote = juce::jlimit(minNote, maxNote, inputNote);

    int bestDistance = std::abs(bestNote - targetNote);



    for (int octaveOffset = -10; octaveOffset <= 10; ++octaveOffset)

    {

        const int candidate = inputNote + (octaveOffset * 12);



        if (candidate < minNote || candidate > maxNote)

            continue;



        const int distance = std::abs(candidate - targetNote);



        if (distance < bestDistance)

        {

            bestDistance = distance;

            bestNote = candidate;

        }

    }



    return juce::jlimit(0, 127, bestNote);

}



int DivisiCleanAudioProcessor::chooseSingleSourceIndexFromNotes(const std::vector<int>& noteNumbers,

    const PresetProfile& profile) const

{

    if (noteNumbers.empty())

        return 0;



    int selectedIndex = 0;



    switch (profile.selectionMode)

    {

    case SourceSelectionMode::Highest:

    {

        int selectedNote = noteNumbers[0];



        for (int i = 1; i < static_cast<int>(noteNumbers.size()); ++i)

        {

            if (noteNumbers[(size_t)i] > selectedNote)

            {

                selectedNote = noteNumbers[(size_t)i];

                selectedIndex = i;

            }

        }



        break;

    }



    case SourceSelectionMode::Lowest:

    {

        int selectedNote = noteNumbers[0];



        for (int i = 1; i < static_cast<int>(noteNumbers.size()); ++i)

        {

            if (noteNumbers[(size_t)i] < selectedNote)

            {

                selectedNote = noteNumbers[(size_t)i];

                selectedIndex = i;

            }

        }



        break;

    }



    case SourceSelectionMode::ClosestToTarget:

    {

        int bestDistance = std::abs(noteNumbers[0] - profile.targetNote);



        for (int i = 1; i < static_cast<int>(noteNumbers.size()); ++i)

        {

            const int distance = std::abs(noteNumbers[(size_t)i] - profile.targetNote);



            if (distance < bestDistance)

            {

                bestDistance = distance;

                selectedIndex = i;

            }

        }



        break;

    }



    default:

        break;

    }



    return selectedIndex;

}



std::vector<int> DivisiCleanAudioProcessor::chooseBlockVoicingIndicesFromNotes(const std::vector<int>& noteNumbers,

    const PresetProfile& profile) const

{

    std::vector<int> indices;



    if (noteNumbers.empty())

        return indices;



    indices.reserve(noteNumbers.size());



    for (int i = 0; i < static_cast<int>(noteNumbers.size()); ++i)

        indices.push_back(i);



    const int maxVoices = juce::jlimit(1, 16, profile.maxVoices);



    auto sortLowToHigh = [&noteNumbers](std::vector<int>& values)

        {

            std::sort(values.begin(), values.end(),

                [&noteNumbers](int a, int b)

                {

                    return noteNumbers[(size_t)a] < noteNumbers[(size_t)b];

                });

        };



    auto sortHighToLow = [&noteNumbers](std::vector<int>& values)

        {

            std::sort(values.begin(), values.end(),

                [&noteNumbers](int a, int b)

                {

                    return noteNumbers[(size_t)a] > noteNumbers[(size_t)b];

                });

        };



    switch (profile.reductionMode)

    {

    case SourceReductionMode::LowestN:

    {

        sortLowToHigh(indices);



        if (static_cast<int>(indices.size()) > maxVoices)

            indices.resize((size_t)maxVoices);



        break;

    }



    case SourceReductionMode::HighestN:

    {

        sortHighToLow(indices);



        if (static_cast<int>(indices.size()) > maxVoices)

            indices.resize((size_t)maxVoices);



        break;

    }



    case SourceReductionMode::Spread:

    {

        // Original DivisiClean block behavior:

        // sort source notes low -> high, then select an evenly distributed

        // subset when there are more notes than the profile allows.

        sortLowToHigh(indices);



        if (static_cast<int>(indices.size()) > maxVoices)

        {

            std::vector<int> reduced;

            reduced.reserve((size_t)maxVoices);



            if (maxVoices == 1)

            {

                reduced.push_back(indices.back());

            }

            else

            {

                const int lastSourceIndex = static_cast<int>(indices.size()) - 1;

                const int lastTargetIndex = maxVoices - 1;



                for (int i = 0; i < maxVoices; ++i)

                {

                    const float ratio = static_cast<float>(i) / static_cast<float>(lastTargetIndex);

                    const int sourcePosition = juce::roundToInt(ratio * static_cast<float>(lastSourceIndex));



                    reduced.push_back(indices[(size_t)sourcePosition]);

                }

            }



            indices = reduced;



            // Preserve low -> high source order after reduction.

            sortLowToHigh(indices);

        }



        break;

    }



    case SourceReductionMode::ClosestToTarget:

    {

        std::sort(indices.begin(), indices.end(),

            [&noteNumbers, &profile](int a, int b)

            {

                const int distanceA = std::abs(noteNumbers[(size_t)a] - profile.targetNote);

                const int distanceB = std::abs(noteNumbers[(size_t)b] - profile.targetNote);



                if (distanceA == distanceB)

                    return noteNumbers[(size_t)a] < noteNumbers[(size_t)b];



                return distanceA < distanceB;

            });



        if (static_cast<int>(indices.size()) > maxVoices)

            indices.resize((size_t)maxVoices);



        sortLowToHigh(indices);

        break;

    }



    case SourceReductionMode::AsPlayed:

    {

        if (static_cast<int>(indices.size()) > maxVoices)

            indices.resize((size_t)maxVoices);



        break;

    }



    case SourceReductionMode::None:

    default:

    {

        // Safe fallback: preserve old deterministic behavior as Spread.

        sortLowToHigh(indices);



        if (static_cast<int>(indices.size()) > maxVoices)

        {

            std::vector<int> reduced;

            reduced.reserve((size_t)maxVoices);



            if (maxVoices == 1)

            {

                reduced.push_back(indices.back());

            }

            else

            {

                const int lastSourceIndex = static_cast<int>(indices.size()) - 1;

                const int lastTargetIndex = maxVoices - 1;



                for (int i = 0; i < maxVoices; ++i)

                {

                    const float ratio = static_cast<float>(i) / static_cast<float>(lastTargetIndex);

                    const int sourcePosition = juce::roundToInt(ratio * static_cast<float>(lastSourceIndex));



                    reduced.push_back(indices[(size_t)sourcePosition]);

                }

            }



            indices = reduced;

            sortLowToHigh(indices);

        }



        break;

    }

    }



    return indices;

}



bool DivisiCleanAudioProcessor::hasEditor() const

{

    return true;

}



juce::AudioProcessorEditor* DivisiCleanAudioProcessor::createEditor()

{

    return new DivisiCleanAudioProcessorEditor(*this);

}



void DivisiCleanAudioProcessor::getStateInformation(juce::MemoryBlock& destData)

{

    juce::XmlElement xml("DivisiCleanLabState");



    xml.setAttribute("engineMode",

        engineModeToStorageString(getEngineMode()));



    copyXmlToBinary(xml, destData);

}



void DivisiCleanAudioProcessor::setStateInformation(const void* data, int sizeInBytes)

{

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));



    if (xmlState == nullptr)

        return;



    if (!xmlState->hasTagName("DivisiCleanLabState"))

        return;



    const auto storedMode = xmlState->getStringAttribute("engineMode", "chordWindow");
    setEngineMode(engineModeFromStorageString(storedMode));
    loadJsonProfiles();

}



//==============================================================================

// This creates new instances of the plugin..

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()

{

    return new DivisiCleanAudioProcessor();

}

