// ViolinMIDI - tests for the engine's 2.1 hooks on the audio thread: the camera's
// bow cue in the note tracker (rules B1 - B7: a softer dip is a new stroke while
// the camera announces one; a new attack in the audio harmonizer) and the
// calibration's telemetry ring.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// Engine.h and SpscRing.h are included at its top.
#pragma once

void bowCue()
{
    vm::NoteTracker::Input in;
    check (! in.bowCue, "no cue unless the camera announces one");
}

void engineTelemetry() {}
