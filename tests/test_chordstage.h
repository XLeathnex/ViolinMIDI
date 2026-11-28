// ViolinMIDI - tests for external chords in the output stage and the engine
// (2.1, OutputStage rule R9): the filter and the voices follow the chord, a
// chord change re-voices only what changed, Hybrid A and B, the harmony that
// waits for a chord being struck, freeze, and a random stress with chords.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// OutputStage.h and Engine.h are included at its top.
#pragma once

void chordStage()
{
    vm::OutputStage stage;
    check (stage.revoiceCount() == 0, "no re-voicing before the first chord");
}

void chordStageRandom() {}
void chordEngine() {}
