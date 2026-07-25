# DivisiClean / Divisimate Knowledge Base

Version: `v0.2`  
Date: 2026-07-25  
Project: **DivisiClean** MIDI utility plugin for Divisimate  
Engine status: **DivisiClean BarLookahead milestone validated**

---

## CRITICAL DIVISIMATE SETTINGS

Divisimate must be configured as follows for the BarLookahead / CC31 packet engine:

```text
Performance Settings → Preset Change
Wait for Sustain Pedal and Release of All Notes: OFF
Transition Behaviour: Overlap
```

This is a major validated milestone.

### Why this matters

Divisimate's `Overlap` transition mode means:

```text
Currently playing notes continue playing in the old orchestration.
Any new notes played after the preset change use the new orchestration.
```

This perfectly matches DivisiClean's CC31-tagged packet model:

```text
Old notes remain attached to the old CC31/profile.
New notes are emitted after the intended CC31 and use the new Divisimate preset.
```

Avoid `Merge` as the default for DivisiClean BarLookahead testing. `Merge` may reinterpret currently active notes into the new orchestration, which can create extra notes, wrong voicing, or apparent preset mismatch.

Avoid `Retrigger` as default unless intentionally desired. It forcibly stops and retriggers active notes in the new orchestration.

---

## CRITICAL DIVISICLEAN ENGINE RULES

1. **Do not reset on CC31.**
2. **Do not panic/clear buffers on CC31 changes.**
3. **Tag notes with the active CC31 at note-on.**
4. **Process completed material grouped by captured CC31.**
5. **Emit CC31 before the corresponding delayed notes.**
6. **Use Divisimate `Overlap` mode.**
7. **Keep CC31 and note timing deterministic.**

This combination has been validated under pressure, including CC31 changes faster than bar boundaries.

---

## 1. Project Overview

**DivisiClean** is a MIDI utility plugin used as a pre-conditioner before **Divisimate**.

Its purpose is to:

- clean and reduce incoming MIDI note material,
- restrict notes to useful orchestral source ranges,
- prepare correct voice counts for each Divisimate preset,
- support live switching through `CC31`,
- avoid muddy voicings,
- compensate for or intentionally exploit Divisimate transposers,
- make complex Divisimate presets playable in real time,
- provide a BarLookahead packet engine for stable delayed orchestration switching.

DivisiClean does **not** replace Divisimate.  
It prepares MIDI so that Divisimate receives musically meaningful input.

---

## 2. Current Engine Status

Current validated engine milestone:

```text
DivisiClean BarLookahead CC31 packet model validated
```

Previous stable reference:

```text
DivisiClean v0.8.1
```

Validated features:

- Live `CC31` profile switching works.
- JSON profile reload works.
- Safe reloads trigger MIDI panic at the next audio block boundary.
- Multi-voice input works.
- `BlockVoicing` works for stacked/chordal presets.
- `SingleSource` works for melody/unison/octave presets.
- `TopDown`, `BottomUp`, and `FillVoices` modes are supported.
- JSON profiles include Divisimate mode metadata.
- BarLookahead can delay, quantize, clean, and emit material later.
- Notes are tagged with the CC31 active at capture time.
- Completed bars/material can be processed by captured CC31 rather than global CC31.
- CC31 changes no longer cause destructive buffer resets.
- With Divisimate set to `Overlap`, preset switching is stable even under faster CC31 changes.

---

## 3. Major Milestone: CC31-Tagged BarLookahead Packet Model

The old problematic behavior was:

```text
incoming CC31 change
→ panic/reset
→ scheduled events cleared
→ bar buffer cleared
→ one-bar gap / lost material
```

The new validated behavior is:

```text
incoming CC31
→ update DivisiClean internal activeCC31 only
→ do not output immediately in BarLookahead mode
→ do not reset
→ do not panic

incoming note-on
→ store note with captured activeCC31

completed material
→ group notes by captured CC31
→ process each group using the matching profile
→ schedule matching CC31 before those delayed notes
→ emit notes into Divisimate in musical sync
```

This solves:

- gaps after preset changes,
- old buffered notes being interpreted through the wrong profile,
- destructive CC31 switching,
- global-state contamination.

---

## 4. BarInputNote Structure

Current BarLookahead notes must carry CC31 identity:

```cpp
struct BarInputNote
{
    int channel = 1;
    int inputNote = 0;
    int velocity = 1;
    double startPpq = 0.0;
    double endPpq = 0.0;
    bool hasEnd = false;

    int cc31 = -1;
};
```

The important rule:

```text
cc31 is captured at note-on time.
```

---

## 5. CC31 Handling in BarLookahead Mode

In `processBarLookaheadBlock()`, CC31 should be handled non-destructively:

```cpp
if (message.isController())
{
    if (message.getControllerNumber() == 31)
    {
        activeCC31.store(message.getControllerValue());
        continue;
    }

    outputMidi.addEvent(message, metadata.samplePosition);
    continue;
}
```

Important:

```text
Do not send CC31 immediately while playing in BarLookahead mode.
Do not clear scheduledLookaheadEvents.
Do not clear barInputNotes.
Do not clear pendingNotes.
Do not reset activeNoteMap.
Do not send MIDI panic.
```

While transport is stopped, it is acceptable to pass CC31 through and also update `activeCC31`, because there is no active delayed musical timeline.

---

## 6. Note Capture in BarLookahead Mode

Note-ons must be tagged:

```cpp
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
```

This is the core of the packet-tagged architecture.

---

## 7. Divisimate Overlap Mode Discovery

A crucial discovery from the Divisimate manual and testing:

### Divisimate Preset Change options

```text
Wait for Sustain Pedal and Release of All Notes
Transition Behaviour: Merge / Overlap / Retrigger
```

### Correct setting for DivisiClean

```text
Wait for Sustain Pedal and Release of All Notes: OFF
Transition Behaviour: Overlap
```

### Manual meaning of Overlap

```text
Currently playing notes continue playing in the old orchestration.
Any new notes played after the preset change are performed by the new orchestration.
```

This is exactly the behavior DivisiClean needs.

### Why Merge is dangerous

`Merge` tries to intelligently transfer active notes into the new orchestration. For DivisiClean this is undesirable because DivisiClean has already authored the musical packet.

Symptoms observed or suspected with Merge:

- apparent stuck/late preset,
- white border/pending-looking preset behavior,
- single-source presets receiving too much material,
- old notes being reinterpreted into the new orchestration.

### Why Overlap works

`Overlap` lets old notes remain with the previous orchestration and new notes use the new CC31 preset. This matches captured-note CC31 ownership.

Pressure test result:

```text
CC31 changes every dotted half note worked well.
Bars were no longer the limiting concept.
Overlap mode allowed the CC31-tagged engine to behave musically.
```

---

## 8. Timing / Preset Lead

A small CC31 lead before notes is useful:

```cpp
const double presetLeadBeats = 0.02; // or 0.04 if needed
```

Approximate values at 110 BPM:

```text
0.01 beats ≈ 5.45 ms
0.02 beats ≈ 10.9 ms
0.04 beats ≈ 21.8 ms
0.06 beats ≈ 32.7 ms
```

Testing showed that simply increasing preset lead does not solve problems caused by Divisimate transition mode. The correct Divisimate transition mode is more important than shaving 10 ms.

Recommended starting point after Overlap validation:

```cpp
const double presetLeadBeats = 0.02;
```

Use `0.04` if extra safety is needed.

---

## 9. Recommended Future Code Hardening

These are recommended but not all mandatory immediately.

### 9.1 CC31 deduplication

Avoid repeatedly sending the same CC31 if Divisimate is already on that preset.

Possible private member:

```cpp
int lastScheduledLookaheadCC31 = -999;
```

Reset in `resetBarLookaheadState()`:

```cpp
lastScheduledLookaheadCC31 = -999;
```

Scheduling logic:

```cpp
if (cc31 == lastScheduledLookaheadCC31)
    return;

lastScheduledLookaheadCC31 = cc31;
```

### 9.2 Sort comparator epsilon

Avoid exact floating-point equality:

```cpp
if (std::abs(a.targetPpq - b.targetPpq) < 0.000001)
```

Ordering priority:

```text
CC31 controllers before notes
controllers before notes
note-offs before note-ons
then PPQ order
```

### 9.3 Verify no duplicate MIDI input path

Divisimate should receive only the intended DivisiClean output during testing.

Avoid:

```text
raw keyboard → Divisimate
and
keyboard → DivisiClean → Divisimate
```

Duplicate input paths can make DivisiClean appear to output too many notes.

### 9.4 Remote Setup recommendation

Divisimate manual says both CC31 and Program Change can load pads 1–100.

Recommended:

```text
CC31 Remote: ON
Program Change Remote: OFF unless intentionally used
```

---

## 10. Important Divisimate Concepts

Divisimate has exactly three divisi modes:

```text
Top Down
Bottom Up
Fill Voices
```

DivisiClean JSON convention:

```text
Divisimate UI      JSON value
Top Down           TopDown
Bottom Up          BottomUp
Fill Voices        FillVoices
```

Example:

```json
"expectedDivisiMode": "BottomUp"
```

---

## 11. Critical Divisimate Label Interpretation

Divisimate lane labels:

```text
T  = Transposer
TR = Trigger
```

Meaning:

```text
T  affects pitch and must be considered in DivisiClean profile design.
TR does not affect pitch. It is an articulation / keyswitch / trigger command.
```

Important rule:

```text
Never infer pitch transposition from TR.
Only T affects pitch.
```

---

## 12. Source Ranges vs Final Sounding Ranges

DivisiClean `voiceSourceRanges` describe the actual MIDI pitches sent into Divisimate.

Divisimate transposers are applied after DivisiClean.

```text
DivisiClean source pitch + Divisimate T = final sounding pitch
```

Example:

```text
Double Bass lane has T +12.
DivisiClean sends MIDI 36–48.
Final sound becomes 48–60.
```

---

## 13. Profile Types

Main profile types:

```text
BlockVoicing
SingleSource
```

### BlockVoicing

Used for:

- multi-voice stacked presets,
- section divisi,
- chordal textures,
- 2-part / 3-part / 4-part / 5-part configurations.

### SingleSource

Used for:

- melody presets,
- unison presets,
- octave-doubling presets,
- presets where one input note feeds multiple Divisimate lanes.

---

## 14. Source Reduction Modes

```text
LowestN  = keep the lowest N useful notes.
HighestN = keep the highest N useful notes.
Spread   = distribute/choose material across the available span.
```

General usage:

```text
Low-string stacked presets      -> LowestN
Upper-string 2-part presets     -> HighestN often works better
Single-section FillVoices       -> Spread
Full ensemble / section divisi  -> Spread or profile-specific
```

---

## 15. Register Wrap Mode

Current preferred mode for most validated profiles:

```json
"registerWrapMode": "PerVoiceRange"
```

This lets each rank have its own practical orchestral range.

---

## 16. Divisimate Routing Logic

### BottomUp

In `BottomUp`, the lowest input voice routes to the lowest active Divisimate rank/lane.

Example with active lanes:

```text
DB, Cello, Viola
```

Routing:

```text
rank 0 / lowest input  -> DB
rank 1 / middle input  -> Cello
rank 2 / highest input -> Viola
```

### TopDown

Upper ranks activate first.

Validated behavior:

```text
Partial voicings in TopDown correctly activate upper ranks first.
```

### FillVoices

Used for section divisi presets.

Practical design:

```text
Treat FillVoices as section-internal distribution.
Use BlockVoicing + Spread + PerVoiceRange.
```

---

## 17. Yellow Line / Melody Rule

For single-source presets, especially melody/unison presets, the key question is:

```text
What is the lowest practical playable input note?
```

These presets are about:

- one musical source,
- clean melody routing,
- avoiding unusable low notes,
- allowing Divisimate transposers to create intended octave doublings.

---

## 18. Section Divisi / Fill Voices Rule

Presets 65–68 are single-section FillVoices presets:

```text
65 — Violins 1
66 — Violins 2
67 — Violas
68 — Cellos
```

They use:

```text
BlockVoicing
FillVoices
Spread
PerVoiceRange
maxVoices: 4
```

Purpose:

```text
Allow one section to play up to 4 internal divisi voices.
```

These presets use `TR` only, not `T`, so there is no pitch compensation.

---

## 19. Validated / Designed Preset Block: CC31 61–69

| CC31 | Name | Voices | Divisimate Mode | DivisiClean Type | Key Logic |
|---:|---|---:|---|---|---|
| 61 | Vln1 Vln2 2-part | 2 | BottomUp | BlockVoicing | Upper two-voice violin harmony; no T |
| 62 | Vln1 Vln2 Vla stacked | 3 | BottomUp | BlockVoicing | Upper/mid string stack; no T |
| 63 | Vla Vc DB stacked | 3 | BottomUp | BlockVoicing | DB has T +12; source 36–48 sounds 48–60 |
| 64 | Vc DB 2-part | 2 | BottomUp | BlockVoicing | DB has T +12; Cello above |
| 65 | Violins 1 | 4 | FillVoices | BlockVoicing | Vln1 section divisi; TR only |
| 66 | Violins 2 | 4 | FillVoices | BlockVoicing | Vln2 section divisi; TR only |
| 67 | Violas | 4 | FillVoices | BlockVoicing | Viola section divisi; TR only |
| 68 | Cellos | 4 | FillVoices | BlockVoicing | Cello section divisi; anti-mud |
| 69 | Vln1 Vln2 8va | 1 | BottomUp | SingleSource | Vln1 T +12; Vln2 source pitch |

---

## 20. JSON Style Conventions

Typical BlockVoicing profile:

```json
{
  "cc31": 64,
  "name": "Vc DB 2-part",
  "profileType": "BlockVoicing",
  "sourceSelectionMode": "Lowest",
  "sourceReductionMode": "LowestN",
  "expectedDivisiMode": "BottomUp",
  "registerWrapMode": "PerVoiceRange",
  "maxVoices": 2,
  "minNote": 36,
  "maxNote": 67,
  "targetNote": 52,
  "outputTranspose": 0,
  "useChordWindow": true,
  "chordWindowMs": 40,
  "enforceActiveVoiceLimit": true,
  "voiceSourceRanges": [
    {
      "rank": 0,
      "minNote": 36,
      "maxNote": 48,
      "targetNote": 43,
      "outputTranspose": 0
    },
    {
      "rank": 1,
      "minNote": 55,
      "maxNote": 67,
      "targetNote": 60,
      "outputTranspose": 0
    }
  ]
}
```

General defaults:

```text
useChordWindow: true
chordWindowMs: 40
enforceActiveVoiceLimit: true
outputTranspose: 0 unless DivisiClean itself must transpose
```

Important:

```text
outputTranspose in DivisiClean is not the same as Divisimate T.
```

---

## 21. Testing Notes

Validated so far:

```text
Live CC31 switching works.
JSON reload is stable.
Switching between presets on bar changes works.
Same Step pattern can change Divisimate/DivisiClean presets on the fly.
BarLookahead no longer gaps on CC31 changes.
Divisimate Overlap mode works extremely well with the tagged packet engine.
Higher-pressure test with CC31 changing every dotted half note worked impressively.
```

Important conclusion:

```text
The engine is no longer strictly dependent on bar boundaries.
The captured-CC31 note ownership model is strong enough for faster musical preset changes.
```

---

## 22. Future Session Instructions

At the beginning of a future session, upload this file and say:

```text
Please read this first. This is the current DivisiClean / Divisimate project state.
Continue from this knowledge base.
```

Then continue with:

```text
Next preset screenshot is CC31 XX.
```

The assistant should:

1. Read Divisimate mode.
2. Identify active lanes.
3. Distinguish `T` from `TR`.
4. Determine routing by BottomUp / TopDown / FillVoices.
5. Decide whether Divisimate transposition must be compensated or intentionally used.
6. Propose DivisiClean ranges.
7. Propose source reduction mode.
8. Produce a JSON profile if requested.
9. Update the preset atlas.

---

## 23. Core Rules to Preserve

```text
TR does not affect pitch.
T affects pitch.
DivisiClean source ranges describe MIDI sent into Divisimate.
Divisimate transposers happen after DivisiClean.
BottomUp assigns lowest input to lowest active lane/rank.
TopDown assigns upper material to upper ranks first.
FillVoices is used for section divisi.
Low strings need anti-mud spacing.
Upper strings can use closer harmony.
DB +12 presets require special source/final range thinking.
Do not reset on CC31 in BarLookahead mode.
Use Divisimate Overlap mode.
```

---

## 24. Current Roadmap

Continue building the preset library in small validated batches.

Recommended process:

```text
1. Inspect screenshot.
2. Identify CC31 preset number.
3. Document Divisimate mode and active lanes.
4. Mark T vs TR.
5. Define DivisiClean profile.
6. Add to JSON.
7. Test live switching.
8. Update atlas.
```

Keep separate versioning:

```text
Plugin engine: BarLookahead CC31 packet milestone validated
Preset library: strings-map-v0.2 or later
Knowledge base: v0.2
```

Future enhancements to consider:

```text
CC31 deduplication
sort comparator epsilon
selectorCc31 vs divisimateCc31 remapping
cc31SnapMode for nearest defined profile
expanded preset atlas
automated JSON validation
UI documentation for required Divisimate Overlap setting
GitHub versioned releases
```
