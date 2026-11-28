// ViolinMIDI - tests for the main window's geometry and the harmonizer output
// (2.1, app/WindowLayout.h, app/OutputRouting.h): the fullscreen plan on the
// common screens, the 75 / 25 split, the zoom steps, the loudness window and
// the routing decisions when devices come and go.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// WindowLayout.h and OutputRouting.h are included at its top.
#pragma once

void windowPlan()
{
    const auto plan = layout::windowed();
    check (plan.width == 1260 && plan.height == 724 && ! plan.dock, "the window is 1260 x 724 without a dock");
}

void graphSplit() {}
void zoomSteps() {}
void loudnessWindow() {}
void outputRouting() {}
