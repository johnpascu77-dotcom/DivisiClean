/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
}

DivisiCleanAudioProcessor::~DivisiCleanAudioProcessor()
{
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
    pendingNotes.clear();
    activeNoteMap.fill(-1);
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

void DivisiCleanAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // This plugin produces no audio.
    buffer.clear();

    const double blockMs = samplesToMs(buffer.getNumSamples());

    for (auto& pending : pendingNotes)
        pending.ageMs += blockMs;

    const auto currentProfile = getProfileForCC31(activeCC31.load());

    // Pass-through profiles:
    // Detect CC31, otherwise leave midiMessages unchanged.
    if (currentProfile.type == ProfileType::PassThrough)
    {
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();

            if (message.isController() && message.getControllerNumber() == 31)
                activeCC31.store(message.getControllerValue());
        }

        return;
    }

    juce::MidiBuffer processedBuffer;

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

            const int outputNote = wrapNoteNearTarget(inputNote,
                currentProfile.minNote,
                currentProfile.maxNote,
                currentProfile.targetNote);

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
    static const PresetProfile profiles[] =
    {
        // CC31 70:
        // Divisimate preset: Vln1 + Vln2 + Vc 8va.
        // SingleSource profile, top-line oriented.
        // v0.5.3 uses chord collection and active cap
        // to guarantee only one melodic/source note reaches Divisimate.
        {
            70,
            "Vln1 + Vln2 + Vc 8va",
            ProfileType::SingleSource,
            SourceSelectionMode::Highest,
            SourceReductionMode::None,
            ExpectedDivisiMode::TopDown,
            1,
            48,
            84,
            72,
            true,
            250.0,
            true
        },

        // CC31 80:
        // Divisimate preset: Strings open 02.
        // Divisimate shows: Bottom Up, 3 voices.
        // DivisiClean currently uses Spread source reduction:
        // it preserves a representative contour from the generated chord,
        // then wraps the selected notes into the preset target range.
        {
            80,
            "Strings open 02",
            ProfileType::BlockVoicing,
            SourceSelectionMode::Highest,
            SourceReductionMode::Spread,
            ExpectedDivisiMode::BottomUp,
            3,
            48,
            84,
            60,
            true,
            250.0,
            true
        },

        // CC31 88:
        // Divisimate preset: Tutti Bass Unison.
        // SingleSource bass-oriented profile.
        // Uses Lowest source selection to guarantee the bass source note.
        {
            88,
            "Tutti Bass Unison",
            ProfileType::SingleSource,
            SourceSelectionMode::Highest,
            SourceReductionMode::None,
            ExpectedDivisiMode::BottomUp,
            1,
            36,
            57,
            48,
            true,
            250.0,
            true
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
        16,
        0,
        127,
        60,
        false,
        0.0,
        false
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

        const int outputNote = wrapNoteNearTarget(inputNote,
            currentProfile.minNote,
            currentProfile.maxNote,
            currentProfile.targetNote);

        auto transformedNoteOn = juce::MidiMessage::noteOn(inputChannel, outputNote, velocity);
        outputMidi.addEvent(transformedNoteOn, 0);

        const int mapIndex = getNoteMapIndex(inputChannel, inputNote);
        activeNoteMap[(size_t)mapIndex] = outputNote;

        return;
    }

    if (currentProfile.type == ProfileType::BlockVoicing)
    {
        const auto selectedIndices = chooseBlockVoicingIndicesFromNotes(noteNumbers, currentProfile);

        std::vector<int> usedOutputNotes;

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
            outputMidi.addEvent(transformedNoteOn, 0);

            const int mapIndex = getNoteMapIndex(inputChannel, inputNote);
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
    juce::ignoreUnused(destData);

    // No persistent state yet.
}

void DivisiCleanAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);

    // No persistent state yet.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DivisiCleanAudioProcessor();
}
