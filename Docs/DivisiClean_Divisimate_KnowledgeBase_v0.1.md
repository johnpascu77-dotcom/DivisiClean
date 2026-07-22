# DivisiClean / Divisimate Knowledge Base

Version: `v0.1`  
Date: 2026-07-22  
Project: **DivisiClean** MIDI utility plugin for Divisimate  
Engine status: **DivisiClean v0.8.1 validated stable**

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
- make complex Divisimate presets playable in real time.

DivisiClean does **not** replace Divisimate.  
It prepares MIDI so that Divisimate receives musically meaningful input.

---

## 2. Current Engine Status

Current validated engine version:

```text
DivisiClean v0.8.1
```

Stable features:

- Live `CC31` profile switching works.
- JSON profile reload works.
- Safe reloads trigger a MIDI panic at the next audio block boundary.
- Multi-voice input works.
- `BlockVoicing` works for stacked/chordal presets.
- `SingleSource` works for melody/unison/octave presets.
- `TopDown`, `BottomUp`, and `FillVoices` modes are supported in `PluginProcessor.cpp`.
- JSON profiles correctly include Divisimate mode metadata.
- Tested live with changing Steps on the bar; preset switching works.

---

## 3. Important Divisimate Concepts

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

## 4. Critical Divisimate Label Interpretation

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

Example:

If a lane has:

```text
TR
```

then DivisiClean should **not** apply compensation.

If a lane has:

```text
T +12
```

then DivisiClean must either:

- pre-compensate the source range, or
- intentionally allow Divisimate to create the octave effect.

---

## 5. Design Convention: Source Ranges vs Final Sounding Ranges

DivisiClean `voiceSourceRanges` describe the **actual MIDI pitches sent into Divisimate**.

Divisimate transposers are applied **after** DivisiClean.

Therefore:

```text
DivisiClean source pitch + Divisimate T = final sounding pitch
```

Example:

```text
Double Bass lane has T +12.
DivisiClean sends MIDI 36–48.
Final sound becomes 48–60.
```

This is central to the preset design.

---

## 6. Profile Types

Main profile types used so far:

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

Typical fields:

```json
"profileType": "BlockVoicing",
"sourceSelectionMode": "Lowest",
"sourceReductionMode": "LowestN",
"registerWrapMode": "PerVoiceRange",
"maxVoices": 3
```

### SingleSource

Used for:

- melody presets,
- unison presets,
- octave-doubling presets,
- presets where one input note feeds multiple Divisimate lanes.

Typical logic:

```text
one musical input source -> multiple routed instruments
```

---

## 7. Source Reduction Modes

Observed / used design logic:

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

Musical reasoning:

- Low strings need body but must avoid mud.
- Upper strings often need the top voices rather than the low chord tones.
- FillVoices section presets benefit from distribution across usable sub-ranges.

---

## 8. Register Wrap Mode

Current preferred mode for most validated profiles:

```json
"registerWrapMode": "PerVoiceRange"
```

This lets each rank have its own practical orchestral range.

Example:

```json
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
```

---

## 9. Divisimate Routing Logic

### BottomUp

In `BottomUp`, the lowest input voice routes to the lowest active Divisimate rank/lane.

For strings, if active lanes are:

```text
DB, Cello, Viola
```

then:

```text
rank 0 / lowest input  -> DB
rank 1 / middle input  -> Cello
rank 2 / highest input -> Viola
```

### TopDown

In `TopDown`, upper ranks activate first.

Important validated behavior:

```text
Partial voicings in TopDown correctly activate upper ranks first.
```

### FillVoices

Used for section divisi presets.

DivisiClean supports:

```json
"expectedDivisiMode": "FillVoices"
```

Practical design so far:

```text
Treat FillVoices as section-internal distribution.
Use BlockVoicing + Spread + PerVoiceRange.
```

---

## 10. Yellow Line / Melody Rule

For single-source presets, especially melody/unison presets, the main question is:

```text
What is the lowest practical playable input note?
```

These presets are not primarily about block harmony.

They are about:

- one musical source,
- clean melody routing,
- avoiding unusable low notes,
- allowing Divisimate transposers to create intended octave doublings.

---

## 11. Section Divisi / Fill Voices Rule

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

## 12. Validated / Designed Preset Block: CC31 61–69

### CC31 69 — Vln1 Vln2 8va

```text
Mode: BottomUp
Profile: SingleSource
Voices: 1 source
Active lanes: Vln1, Vln2
Vln1 has T +12
Vln2 receives source pitch
```

Musical meaning:

```text
Violin 2 plays the source pitch.
Violin 1 sounds one octave above through Divisimate T +12.
```

DivisiClean does not compensate away the transposer; the octave effect is intentional.

---

### CC31 68 — Cellos

```text
Mode: FillVoices
Profile: BlockVoicing
Section: Cello only
Max voices: 4
TR only, no pitch transposition
```

Suggested range:

```text
Cello section divisi: 48–76
```

Purpose:

```text
Avoid muddy low cello clusters.
```

---

### CC31 67 — Violas

```text
Mode: FillVoices
Profile: BlockVoicing
Section: Viola only
Max voices: 4
TR only, no pitch transposition
```

Suggested range:

```text
Viola section divisi: 55–79
```

---

### CC31 66 — Violins 2

```text
Mode: FillVoices
Profile: BlockVoicing
Section: Violin 2 only
Max voices: 4
TR only, no pitch transposition
```

Suggested range:

```text
Violin 2 section divisi: 67–88
```

---

### CC31 65 — Violins 1

```text
Mode: FillVoices
Profile: BlockVoicing
Section: Violin 1 only
Max voices: 4
TR only, no pitch transposition
```

Suggested range:

```text
Violin 1 section divisi: 67–91
```

---

### CC31 64 — Vc. & DB 2-part

Screenshot interpretation:

```text
Preset: Vc. & DB (2-part)
Mode: BottomUp
Voices: 2
Active lanes: Cello, Double Bass
Cello: TR only
Double Bass: T +12 and TR
```

Routing:

```text
rank 0 / lowest input  -> Double Bass -> T +12
rank 1 / highest input -> Cello       -> unchanged
```

Recommended DivisiClean source ranges:

```text
rank 0 DB source: 36–48 -> final 48–60
rank 1 Cello:     55–67 -> final 55–67
```

Musical purpose:

```text
Clear low-string 2-part harmony.
DB is raised by Divisimate and becomes a low/mid support voice.
Cello sits above it.
```

Good final intervals:

```text
fifths, sixths, octaves, tenths, open voicings
```

Avoid:

```text
low close seconds/thirds
muddy overlapping DB/cello clusters
```

Recommended source reduction:

```text
LowestN
```

---

### CC31 63 — Vla + Vc + DB stacked

Screenshot interpretation:

```text
Preset: Vla + Vc. + DB (stacked)
Mode: BottomUp
Voices: 3
Active lanes: Viola, Cello, Double Bass
Viola: TR only
Cello: TR only
Double Bass: T +12 and TR
```

Routing:

```text
rank 0 / lowest input  -> Double Bass -> T +12
rank 1 / middle input  -> Cello       -> unchanged
rank 2 / highest input -> Viola       -> unchanged
```

Recommended DivisiClean source ranges:

```text
rank 0 DB source: 36–48 -> final 48–60
rank 1 Cello:     55–67 -> final 55–67
rank 2 Viola:     62–74 -> final 62–74
```

Musical purpose:

```text
Clear stacked low/mid string harmony.
DB is raised one octave by Divisimate.
Cello is the middle voice.
Viola is the clarifying top voice.
```

Recommended source reduction:

```text
LowestN
```

Good final harmony:

```text
open triads, fifths, sixths, tenths, quartal/open voicings
```

Avoid:

```text
dense low-register 3-note clusters
```

---

### CC31 62 — Vln1 + Vln2 + Vla stacked

Screenshot interpretation:

```text
Preset: Vln. 1 & Vln. 2 & Vla. stacked
Mode: BottomUp
Voices: 3
Active lanes: Viola, Violin 2, Violin 1
No T transposers
TR only on active lanes
```

Routing:

```text
rank 0 / lowest input  -> Viola
rank 1 / middle input  -> Violin 2
rank 2 / highest input -> Violin 1
```

Recommended DivisiClean source ranges:

```text
rank 0 Viola:    55–72
rank 1 Violin 2: 62–79
rank 2 Violin 1: 67–88
```

Musical purpose:

```text
Clear upper/mid string triads and stacked harmony.
```

Recommended source reduction:

```text
LowestN or profile-specific.
```

Note:

```text
This preset is straightforward because there are no pitch transposers.
```

---

### CC31 61 — Vln1 + Vln2 2-part

Screenshot interpretation:

```text
Preset: Vln. 1 & Vln. 2 (2-part)
Mode: BottomUp
Voices: 2
Active lanes: Violin 2, Violin 1
No T transposers
TR only on active lanes
```

Routing:

```text
rank 0 / lower input  -> Violin 2
rank 1 / upper input  -> Violin 1
```

Recommended DivisiClean source ranges:

```text
rank 0 Violin 2: 60–84
rank 1 Violin 1: 64–91
```

Musical purpose:

```text
Upper-string 2-part harmony.
```

Good intervals:

```text
thirds, sixths, octaves, suspensions, melody harmonization
```

Recommended source reduction:

```text
HighestN
```

Reason:

```text
If more than two notes are played, upper strings often want the upper two notes,
not the lower harmonic body.
```

---

## 13. Validated Preset Table

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

## 14. JSON Style Conventions

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

## 15. Testing Notes

Validated so far:

```text
Live CC31 switching works.
JSON reload is stable.
Switching between recent presets on bar changes works.
Same Step pattern can change Divisimate/DivisiClean presets on the fly.
```

Important test result:

```text
A live test with the last four presets switching on the bar worked successfully.
```

This supports continuing the 100-preset library build in small validated batches.

---

## 16. Future Session Instructions

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

## 17. Core Rules to Preserve

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
```

---

## 18. Current Roadmap

Continue building the preset library downward/upward in small batches.

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
Plugin engine: v0.8.1 currently validated
Preset library: strings-map-v0.1 or later
```

Future enhancements to consider:

```text
selectorCc31 vs divisimateCc31 remapping
cc31SnapMode for nearest defined profile
expanded preset atlas
automated JSON validation
```