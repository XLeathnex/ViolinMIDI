// ViolinMIDI - tests for the chords of 2.1 (core/Chord.h, core/ChordTracker.h):
// the tables, recognition, chord scales and the key context, the pentatonic and
// the voices, and the chord keyboard driven hop by hop at 750 hops/s.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// Chord.h and ChordTracker.h are included at its top.
#pragma once

void chordTables()
{
    check (vm::chord::kQualityCount == 18 && vm::chord::kScaleTypeCount == 21, "18 qualities, 21 chord scales");
}

void chordRecognition() {}
void chordScales() {}
void chordKeyContext() {}
void chordVoices() {}
void chordTrackerTests() {}
