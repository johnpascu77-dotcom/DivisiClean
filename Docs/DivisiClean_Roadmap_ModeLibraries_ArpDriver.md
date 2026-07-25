# DivisiClean Roadmap: Mode Libraries, Tuplet Safety, Meter Awareness, and Divisimate ARP Driver

**Project:** DivisiClean  
**Context:** Divisimate 2 MIDI preprocessing / orchestration assistant  
**Status:** Conceptual development roadmap after successful CC31-tagged BarLookahead engine validation  
**Suggested milestone family:** v0.8.3 → v1.0.0  
**Date:** 2026-07-25

---

## 1. Current Stable Breakthrough

DivisiClean currently has a working and validated architecture based on:

- CC31-selected Divisimate presets
- JSON profile library
- BarLookahead engine
- Delayed output
- CC31 tagging per note at note-on
- Divisimate Overlap mode compatibility

The most important successful principle is:

> A note belongs to the CC31 preset active at its note-on.

This means DivisiClean can handle changing Divisimate presets during the musical stream without confusing old and new notes, as long as the engine tags captured notes correctly and emits the corresponding CC31 before delayed output notes.

Current stable behavior:

```text
CC31 changes are not treated as reset events.
Notes are tagged with active CC31 at note-on.
Each tagged group is processed with its corresponding profile.
CC31 is emitted before the delayed notes that need it.
Divisimate Overlap mode handles old/new preset coexistence well.
```

This is the foundation for all future improvements.

---

## 2. Safety Principle: Do Not Break the Working VST3

The current working `.vst3` should be preserved.

Instead of overwriting the same plugin repeatedly, development should move toward separate plugin products that can live side-by-side in the VST3 folder.

Recommended plugin variants:

```text
DivisiClean.vst3
DivisiClean Lab.vst3
```

Optional later variants:

```text
DivisiClean Live.vst3
DivisiClean BarLookahead.vst3
DivisiClean Generated MIDI.vst3
DivisiClean ARP Driver.vst3
```

At the current phase, the safest approach is:

```text
Keep DivisiClean as stable.
Create DivisiClean Lab as experimental.
```

Important JUCE identity fields must be unique:

```text
Plugin Name
Plugin Code
Bundle Identifier
VST3 identity/product string
```

It is not enough to rename the `.vst3` file. The plugin identity itself must be different, otherwise the DAW may treat different builds as the same plugin.

Suggested IDs:

```text
Stable:
Plugin Name: DivisiClean
Plugin Code: DvCl
Bundle ID: com.divisiclean.plugin

Lab:
Plugin Name: DivisiClean Lab
Plugin Code: DvLb
Bundle ID: com.divisiclean.lab
```

---

## 3. Current Pain Point: One JSON File for Multiple Engine Concepts

Currently, DivisiClean uses one main JSON file:

```text
DivisiCleanProfiles.json
```

This is becoming limiting because different engine modes require different assumptions.

For example:

```text
Live ChordWindow mode wants human-tolerant chord grouping.
BarLookahead mode wants one-bar delayed packet processing.
Generated MIDI mode wants exact PPQ timing and tuplet safety.
Divisimate ARP Driver mode wants long clean input notes, not final rhythms.
```

Therefore, the project should evolve toward mode-specific profile libraries.

---

## 4. Recommended Profile File Structure

Runtime location:

```text
C:\Users\Asus\Documents\Divisiclean\Profiles\
```

Repository location:

```text
Profiles\
```

Recommended files:

```text
DivisiCleanProfiles.json
DivisiCleanProfiles_BarLookahead.json
DivisiCleanProfiles_ChordWindow.json
DivisiCleanProfiles_GeneratedMidi.json
DivisiCleanProfiles_MeterAware.json
DivisiCleanProfiles_ArpDriver.json
```

The existing `DivisiCleanProfiles.json` should remain as a fallback for backward compatibility.

The mode-specific files can initially be copies of the current profile library, then gradually enriched.

Example starting commands from the repository root:

```cmd
copy Profiles\DivisiCleanProfiles.json Profiles\DivisiCleanProfiles_BarLookahead.json
copy Profiles\DivisiCleanProfiles.json Profiles\DivisiCleanProfiles_ChordWindow.json
copy Profiles\DivisiCleanProfiles.json Profiles\DivisiCleanProfiles_GeneratedMidi.json
copy Profiles\DivisiCleanProfiles.json Profiles\DivisiCleanProfiles_ArpDriver.json
```

---

## 5. GUI Mode Selector

A future DivisiClean GUI should allow selecting the operating mode.

Suggested modes:

```text
Live ChordWindow
BarLookahead
Meter-Aware BarLookahead
Generated MIDI / Tuplet Safe
Divisimate ARP Driver
```

The mode selector should not necessarily change processing live during playback.

Recommended safe workflow:

```text
1. Stop playback.
2. Select mode in the GUI.
3. Press Reload Mode JSON + Panic.
4. DivisiClean clears buffers and scheduled events.
5. DivisiClean loads the selected JSON file.
6. GUI updates the displayed engine/profile state.
```

Suggested GUI display:

```text
Mode: BarLookahead
Library: DivisiCleanProfiles_BarLookahead.json
[Reload Mode JSON + Panic]
```

For ARP Driver mode:

```text
Engine: Divisimate ARP Driver | Delay: 1.0 bar
Input: Long Notes | Rhythm: Divisimate
ARP: ON | Melodic Seq: OFF
```

---

## 6. Engine Modes

### 6.1 Live ChordWindow

Purpose:

```text
Human live playing with minimal latency.
```

Behavior:

```text
Use chordWindowMs to collect human chord flams.
No one-bar delay.
Process quickly.
Good for keyboard playing and sketching.
```

Typical JSON file:

```text
DivisiCleanProfiles_ChordWindow.json
```

Recommended display:

```text
Engine: Live ChordWindow
Window: 40 ms
Delay: 0 bar
Timing: Human
```

---

### 6.2 BarLookahead

Purpose:

```text
Delay by one bar so DivisiClean can analyze/clean material before sending it to Divisimate.
```

Behavior:

```text
Capture incoming notes.
Tag each note with active CC31 at note-on.
Process completed bar/packet.
Emit CC31 before corresponding delayed notes.
Use Divisimate Overlap mode.
```

Typical JSON file:

```text
DivisiCleanProfiles_BarLookahead.json
```

Recommended display:

```text
Engine: BarLookahead
Delay: 1.0 bar
Grid: 1/16 bar
Timing: Bar packet
```

---

### 6.3 Meter-Aware BarLookahead

Purpose:

```text
Correctly support 3/4, 5/4, 6/8, 7/8, mixed meter, etc.
```

Key formula:

```text
barLengthInQuarterNotes = numerator * 4.0 / denominator
```

Examples:

```text
4/4  -> 4.0
3/4  -> 3.0
6/8  -> 3.0
7/8  -> 3.5
5/4  -> 5.0
```

Preferred host data:

```text
Current PPQ position
PPQ position of last bar start
Current time signature
```

Important design rule:

```text
Do not hardcode 4.0 beats per bar.
```

Possible JSON file:

```text
DivisiCleanProfiles_MeterAware.json
```

This may eventually become the default implementation of BarLookahead.

---

### 6.4 Generated MIDI / Tuplet Safe

Purpose:

```text
Preserve clean generated MIDI timing, including tuplets, ostinati, arpeggios, and algorithmic patterns.
```

Core principle:

```text
DivisiClean should be pitch/voicing transformative but rhythm transparent.
```

Meaning:

```text
It may reduce notes.
It may wrap registers.
It may choose voices.
It may select profiles by CC31.
But it should not alter rhythmic timing unless explicitly requested.
```

Important behavior:

```text
Do not quantize PPQ positions.
Preserve original note-on and note-off timing.
Use exact or epsilon-based same-timestamp grouping.
Process CC31 before notes at the same PPQ.
```

Useful grouping modes:

```text
HumanWindowMs
ExactTimestamp
PpqEpsilon
```

Suggested JSON settings:

```json
{
  "engineDefaults": {
    "engineMode": "GeneratedMidi",
    "chordGroupingMode": "PpqEpsilon",
    "chordWindowPpq": 0.0005,
    "rhythmHandling": "Preserve",
    "sameTimestampCcBeforeNotes": true
  }
}
```

Typical JSON file:

```text
DivisiCleanProfiles_GeneratedMidi.json
```

---

### 6.5 Divisimate ARP Driver

Purpose:

```text
Feed Divisimate's internal Arpeggiator and Melodic Sequencer with clean input material.
```

This is a major conceptual expansion.

Divisimate presets may have:

```text
T = Transposer
A = Arpeggiator
```

The `A` means that Divisimate itself rhythmically modifies notes routed to that instrument.

In Expert mode, Divisimate's ARP can also include a melodic sequencer, which changes the pitch pattern of incoming notes.

Therefore, for these presets, DivisiClean should not always preserve input rhythm as final rhythm.

Instead:

```text
Input MIDI = harmonic/control material
Divisimate ARP = rhythmic realization
Divisimate Melodic Sequencer = pitch-pattern realization
```

Recommended mode name:

```text
Divisimate ARP Driver
```

or simply:

```text
ARP Driver
```

Typical JSON file:

```text
DivisiCleanProfiles_ArpDriver.json
```

---

## 7. Divisimate ARP Driver Concept

In ARP Driver mode, DivisiClean prepares clean note material for Divisimate.

Possible behavior:

```text
Receive chord or anchor notes.
Reduce to desired number of voices.
Wrap notes into useful ranges.
Send CC31 preset before notes.
Hold or extend notes so Divisimate ARP can perform them.
Avoid sending dense pre-arpeggiated rhythms unless intended.
Retrigger only when harmony changes.
Release notes at next harmony change or bar end.
```

The key difference:

```text
Normal modes preserve rhythm.
ARP Driver mode may generate/control sustained anchors for Divisimate to rhythmize.
```

Important warning:

```text
Do not feed already dense rhythms into a Divisimate ARP preset unless intentional.
Rhythm into rhythm generator can create chaos or over-patterning.
```

---

## 8. Melodic Sequencer Awareness

Divisimate's Expert ARP mode can change pitch.

Example conceptual pattern:

```text
Input note: C
Divisimate melodic sequence: 0, +3, -5, -7, +4
Resulting notes: C, Eb, G, F, E, etc.
```

Therefore, DivisiClean should sometimes treat input notes as:

```text
Pitch anchors
Harmony anchors
Control notes
```

rather than final notes.

Suggested JSON flags:

```json
{
  "usesArpeggiator": true,
  "usesMelodicSequencer": true,
  "rhythmGeneratedByDivisimate": true,
  "pitchGeneratedByDivisimate": true,
  "inputExpectation": "AnchorNotes"
}
```

For rhythm-only ARP presets:

```json
{
  "usesArpeggiator": true,
  "usesMelodicSequencer": false,
  "rhythmGeneratedByDivisimate": true,
  "pitchGeneratedByDivisimate": false,
  "inputExpectation": "LongNotes"
}
```

---

## 9. Avoid Starting the JSON Library from Zero

Very important principle:

> The existing JSON library should remain the basis.

The current library already contains the hardest work:

```text
CC31 number
Profile name
Profile type
Source reduction mode
Max voices
Expected divisi mode
Register wrap mode
Transposer/range assumptions
Instrument/preset identity
```

The new work should enrich this library with optional metadata, not replace it.

The ARP/sequencer behavior can be added gradually.

---

## 10. Preset Family Templates

To avoid editing every preset manually, use reusable preset-family templates.

Many Divisimate rhythm presets are systematic.

Example from Universal Orchestral Rhythms:

```text
WW High 8ths Offbeats
WW Reed Low 8ths Offbeats
WW Mixed 8ths Offbeats
Full WW 8ths Offbeats
Trumpets 8ths Offbeats
Horns 8ths Offbeats
Trombones 8ths Offbeats
Full Brass 8ths Offbeats
High Strings 8ths Offbeats
Low Strings 8ths Offbeats
Full Strings 8ths Offbeats
Mixed Warm 8ths Offbeats
Mixed Sparse 8ths Offbeats
Mixed Open 8ths Offbeats
Mixed Dense 8ths Offbeats
Mixed Deep 8ths Offbeats
Mixed Wide 8ths Offbeats
Mixed Tutti 8ths Offbeats
```

These can share one family definition:

```json
{
  "presetFamilies": {
    "8thsOffbeats": {
      "description": "Divisimate ARP-generated eighth-note offbeat orchestral rhythm.",
      "usesArpeggiator": true,
      "usesMelodicSequencer": false,
      "rhythmGeneratedByDivisimate": true,
      "pitchGeneratedByDivisimate": false,
      "inputExpectation": "LongNotes",
      "recommendedInputLength": "OneBarOrHalfBar",
      "recommendedEngineMode": "ArpDriver",
      "avoidFastInputRhythms": true
    }
  }
}
```

Then individual profiles only need:

```json
{
  "cc31": 43,
  "name": "WW Mixed 8ths Offbeats",
  "divisimateFamily": "8thsOffbeats",
  "maxVoices": 3
}
```

This prevents starting again preset-by-preset.

---

## 11. Proposed JSON Structure

Future mode-specific JSON files can use this structure:

```json
{
  "libraryName": "DivisiClean Universal Orchestral Rhythms",
  "libraryVersion": "0.1",

  "engineDefaults": {
    "engineMode": "ArpDriver",
    "delayBars": 1.0,
    "chordGroupingMode": "ExactTimestamp",
    "rhythmHandling": "DivisimateGenerated",
    "sameTimestampCcBeforeNotes": true
  },

  "presetFamilies": {
    "8thsOffbeats": {
      "description": "Divisimate ARP-generated eighth-note offbeat orchestral rhythm.",
      "usesArpeggiator": true,
      "usesMelodicSequencer": false,
      "rhythmGeneratedByDivisimate": true,
      "pitchGeneratedByDivisimate": false,
      "inputExpectation": "LongNotes",
      "recommendedInputLength": "OneBarOrHalfBar",
      "recommendedEngineMode": "ArpDriver",
      "avoidFastInputRhythms": true
    },

    "MelodicArp": {
      "description": "Divisimate ARP plus melodic sequencer pattern.",
      "usesArpeggiator": true,
      "usesMelodicSequencer": true,
      "rhythmGeneratedByDivisimate": true,
      "pitchGeneratedByDivisimate": true,
      "inputExpectation": "AnchorNotes",
      "recommendedInputLength": "OneBar",
      "recommendedEngineMode": "ArpDriver"
    }
  },

  "profiles": [
    {
      "cc31": 43,
      "name": "WW Mixed 8ths Offbeats",
      "profileType": "MultiTarget",
      "divisimateFamily": "8thsOffbeats",
      "maxVoices": 3,
      "expectedDivisiMode": "BottomUp",
      "sourceReductionMode": "TopN",
      "registerWrapMode": "PerNoteNearTarget"
    }
  ]
}
```

---

## 12. Engine Defaults vs Profile Metadata

Recommended split:

### Engine/global settings

These describe how DivisiClean processes timing and grouping.

```text
engineMode
delayBars
chordGroupingMode
chordWindowMs
chordWindowPpq
sameTimestampCcBeforeNotes
rhythmHandling
```

### Profile/musical settings

These describe the CC31 preset behavior.

```text
cc31
name
profileType
maxVoices
sourceReductionMode
expectedDivisiMode
registerWrapMode
divisimateFamily
usesArpeggiator
usesMelodicSequencer
inputExpectation
```

Mode-specific JSON files may include both.

---

## 13. Backward Compatibility

The loader should remain compatible with the current simple file:

```text
DivisiCleanProfiles.json
```

Recommended loading behavior:

```text
Try selected mode-specific file.
If missing, fall back to DivisiCleanProfiles.json.
If engineDefaults are missing, use safe internal defaults.
If ARP metadata is missing, assume normal rhythm-transparent behavior.
```

This allows gradual migration.

---

## 14. Suggested C++ Enums

Possible future internal enums:

```cpp
enum class EngineMode
{
    ChordWindow,
    BarLookahead,
    MeterAwareBarLookahead,
    GeneratedMidi,
    ArpDriver
};
```

```cpp
enum class ChordGroupingMode
{
    HumanWindowMs,
    ExactTimestamp,
    PpqEpsilon
};
```

```cpp
enum class RhythmHandling
{
    Preserve,
    DivisimateGenerated
};
```

Possible config structure:

```cpp
struct EngineDefaults
{
    EngineMode engineMode = EngineMode::BarLookahead;
    ChordGroupingMode chordGroupingMode = ChordGroupingMode::HumanWindowMs;
    RhythmHandling rhythmHandling = RhythmHandling::Preserve;

    double delayBars = 1.0;
    double presetLeadBeats = 0.02;
    double chordWindowMs = 40.0;
    double chordWindowPpq = 0.0005;

    bool sameTimestampCcBeforeNotes = true;
};
```

---

## 15. Suggested GUI Elements

Add:

```cpp
juce::ComboBox engineModeBox;
```

Suggested items:

```text
Live ChordWindow
BarLookahead
Meter-Aware BarLookahead
Generated MIDI / Tuplet Safe
Divisimate ARP Driver
```

Button:

```text
Reload Mode JSON + Panic
```

Displayed fields:

```text
Mode
Loaded JSON file
Active CC31
Profile name
Profile category
Engine
Grouping
Delay
ARP ON/OFF
Melodic Seq ON/OFF
Input expectation
Buffered notes
Scheduled events
```

Example for normal BarLookahead:

```text
Engine: BarLookahead | Grid: 1/16 bar | Delay: 1.0 bar
Grouping: Human Window | Window: 40 ms
```

Example for Generated MIDI:

```text
Engine: Generated MIDI | Delay: 1.0 bar
Grouping: PPQ Epsilon | Epsilon: 0.0005
Rhythm: Preserve | Tuplet-safe: ON
```

Example for ARP Driver:

```text
Engine: Divisimate ARP Driver | Delay: 1.0 bar
Input: Long Notes | Rhythm: Divisimate
ARP: ON | Melodic Seq: OFF
```

---

## 16. Development Phases

### Phase 1 — Protect Stable Version

Goal:

```text
Keep current working DivisiClean.vst3 untouched.
Create DivisiClean Lab.vst3 as separate plugin product.
```

Tasks:

```text
Change JUCE plugin name/ID for Lab build.
Build and confirm both plugins appear separately in the DAW.
Commit stable state.
Tag stable version.
```

---

### Phase 2 — Mode-Specific JSON File Loading

Goal:

```text
Allow GUI mode selection and load corresponding JSON file.
```

Tasks:

```text
Add engine mode selector.
Map each mode to a JSON filename.
Add Reload Mode JSON + Panic behavior.
Fallback to DivisiCleanProfiles.json if selected file is missing.
Display selected JSON file in GUI.
```

Suggested filenames:

```text
DivisiCleanProfiles_ChordWindow.json
DivisiCleanProfiles_BarLookahead.json
DivisiCleanProfiles_GeneratedMidi.json
DivisiCleanProfiles_ArpDriver.json
```

---

### Phase 3 — Engine Defaults in JSON

Goal:

```text
Allow each JSON library to define best mode settings.
```

Tasks:

```text
Add optional engineDefaults object.
Parse engineMode, delayBars, chordGroupingMode, etc.
Display loaded defaults in GUI.
Use safe defaults if fields are absent.
```

---

### Phase 4 — Generated MIDI / Tuplet Safe Mode

Goal:

```text
Preserve exact generated MIDI timing, including tuplets.
```

Tasks:

```text
Avoid PPQ quantization.
Preserve note-on/note-off PPQ.
Add same-timestamp CC31-before-notes ordering.
Add ExactTimestamp and PpqEpsilon grouping.
Test clean tuplets from a MIDI generator.
```

---

### Phase 5 — Meter-Aware BarLookahead

Goal:

```text
Support non-4/4 and mixed meter correctly.
```

Tasks:

```text
Read host time signature.
Read host PPQ position of last bar start.
Calculate bar length from numerator and denominator.
Store bar start/end per captured packet.
Test 3/4, 5/4, 6/8, 7/8, mixed meter.
```

---

### Phase 6 — Divisimate ARP Metadata

Goal:

```text
Add awareness of Divisimate ARP and melodic sequencer presets.
```

Tasks:

```text
Add optional profile fields:
    divisimateFamily
    usesArpeggiator
    usesMelodicSequencer
    rhythmGeneratedByDivisimate
    pitchGeneratedByDivisimate
    inputExpectation

Add presetFamilies template section.
Display ARP metadata in GUI.
Do not change processing behavior yet.
```

---

### Phase 7 — ARP Driver Processing

Goal:

```text
Feed Divisimate ARP presets with clean long notes / anchor notes.
```

Possible behavior:

```text
Extend notes to beat/bar end.
Retrigger only on harmony change.
Avoid passing dense input rhythm into ARP presets.
Send panic/reset safely on mode reload.
Send CC31 before anchor notes.
Release previous anchors cleanly.
```

This phase should be approached carefully after metadata is reliable.

---

### Phase 8 — Future v1.0 Architecture

Possible mature architecture:

```text
One master profile atlas
+
Separate mode preset files
```

Instead of duplicating full JSON libraries, use:

```text
DivisiCleanProfiles_Master.json
Modes\BarLookaheadMode.json
Modes\GeneratedMidiMode.json
Modes\ArpDriverMode.json
```

The loader would merge:

```text
master musical profiles
+
selected mode defaults
+
selected preset family metadata
```

This is cleaner long-term, but more complex. It should wait until the simpler mode-specific JSON approach is stable.

---

## 17. Version Roadmap

Conservative versioning:

```text
v0.8.2 — Current validated CC31-tagged BarLookahead / Overlap engine
v0.8.3 — Separate Lab plugin product
v0.8.4 — Mode-specific JSON libraries
v0.8.5 — Engine defaults from JSON
v0.8.6 — Generated MIDI / Tuplet Safe grouping
v0.8.7 — Meter-aware BarLookahead
v0.8.8 — Divisimate ARP metadata display
v0.8.9 — Initial ARP Driver behavior
v1.0.0 — Unified atlas + mode presets
```

Alternative larger milestone names:

```text
v0.9.0 — Mode Libraries
v0.9.1 — Tuplet Safe / Generated MIDI
v0.9.2 — Meter-Aware BarLookahead
v0.9.3 — Divisimate ARP Metadata
v1.0.0 — Divisimate Intelligence Layer
```

---

## 18. Key Design Principles

### Preserve the stable version

```text
Never risk the current working plugin when experimenting.
```

### Existing JSON remains valuable

```text
Do not restart preset-by-preset.
Use the current 100-profile library as the base atlas.
```

### Add metadata, do not replace profiles

```text
ARP/sequencer behavior should be optional metadata layered onto existing profiles.
```

### DivisiClean should not duplicate Divisimate

```text
Divisimate owns its internal ARP and melodic sequencer patterns.
DivisiClean only needs to know how to feed those presets correctly.
```

### Rhythm transparency by default

```text
Normal modes should preserve input rhythm.
```

### ARP Driver is different

```text
In ARP Driver mode, input notes are anchors/control material, not necessarily final rhythm.
```

### Use templates to avoid manual work

```text
Preset families such as 8thsOffbeats should share metadata through templates.
```

### Safe reload

```text
Mode change should clear buffers and scheduled events.
Use Reload Mode JSON + Panic.
```

---

## 19. Immediate Next Steps

The next practical steps should be:

1. Commit and tag the current stable version.
2. Create a separate Lab plugin product.
3. Confirm stable and Lab VST3 can coexist.
4. Add/copy mode-specific JSON files.
5. Add GUI mode selector.
6. Add mode-to-JSON loading logic.
7. Add safe reload/panic behavior.
8. Only then begin adding Generated MIDI, meter-aware, and ARP Driver intelligence.

Recommended first milestone:

```text
v0.8.3 — DivisiClean Lab product and mode-library foundation
```

---

## 20. One-Sentence Vision

DivisiClean should become a preset-aware intelligence layer for Divisimate:

```text
It receives musical input, selects the right Divisimate preset behavior via CC31, cleans and shapes the input according to the profile library, and feeds Divisimate in the form each preset expects — live notes, delayed packets, generated-MIDI precision, or ARP/sequencer anchor material.
```
