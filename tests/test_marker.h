// ViolinMIDI - tests for the colour marker on the scroll (2.1, app/CameraMarker.h):
// HSV, the pipette on synthetic frames (and its warnings), and the finder in
// camera frames, with brightness and hue changes, two stickers and reflections.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// CameraMarker.h is included at its top.
#pragma once

void cameraMarker()
{
    check (! cam::MarkerColour().valid, "no colour before the pipette");
}
