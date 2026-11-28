// The geometry of the main window (2.1): the plan for fullscreen (how much
// bigger everything is drawn, the logical size it is laid out at, whether the
// camera panel is docked at the right and whether the columns are paged), the
// 75 / 25 split of the live display, its zoom steps and the loudness window.
//
// Plain C++ without JUCE, so every number is tested in violinmidi_tests.
// WindowRoot computes the plan, MainComponent lays out from it.
#pragma once

namespace layout
{

// --- the window (2.0's size, unchanged)
constexpr int kWidth = 1260, kHeight = 724;
constexpr int kHeaderHeight = 44, kStatusHeight = 40;
constexpr int kColumnsBand = 340;          ///< 20 gap + title 18 + 12 + 5 items of 58
constexpr int kPagedColumnsBand = 360;     ///< plus the page switch
constexpr int kDisplayHeight = kHeight - kHeaderHeight - kStatusHeight - kColumnsBand;   ///< 300

// --- fullscreen
constexpr int   kRefHeight = 800;          ///< 724 + 76: the display grows by a quarter
constexpr int   kDockWidth = 440, kDockRule = 1;   ///< the docked camera panel and its hairline
constexpr int   kPagedMainMin = 760;       ///< three columns
constexpr float kMinGrowth = 1.10f;        ///< below this the wide layout with dock gives way to the paged one
constexpr float kMinScale = 0.75f, kMaxScale = 2.5f;
constexpr float kMinPanelScale = 0.85f;    ///< below: the docked panel scrolls

/** Where everything goes, in the logical coordinates the main view is laid out in. */
struct Plan
{
    bool  fullscreen = false;
    float scale = 1.0f;                    ///< logical -> screen
    float offsetX = 0.0f, offsetY = 0.0f;  ///< the remainder of less than one logical pixel, halved
    int   width = kWidth, height = kHeight;   ///< logical size of the main view
    bool  dock = false;                    ///< the camera panel sits at the right
    bool  paged = false;                   ///< three columns at a time, with a page switch
    int   mainWidth = kWidth;              ///< without the dock
    int   displayHeight = kDisplayHeight;
    int   dockHeight = 0;                  ///< between header and status
    float panelScale = 1.0f;               ///< of the docked panel inside the dock
    bool  panelScrolls = false;
};

/** 1260 x 724, scale 1, no dock: the window as in 2.0. */
Plan windowed() noexcept;

/** Fullscreen on a content area of `width` x `height` points; `dock`: the camera panel is wanted
    at the right; `panelHeight`: CameraPanel::kHeight. */
Plan fullscreen (int width, int height, bool dock, int panelHeight) noexcept;

//==============================================================================
// The live display

// LiveDisplay's margins around the two graphs (Widgets.cpp uses the same).
constexpr int kGraphTop = 16, kGraphBottom = 14, kTimeRowHeight = 16, kLevelGap = 10;

/** Pitch 75 %, loudness 25 % of the graph height of a display this high. */
struct GraphSplit { int graph = 0, pitch = 0, level = 0; };
GraphSplit graphSplit (int displayHeight) noexcept;

constexpr int kZoomSteps = 5;
constexpr int kStandardZoom = 2;           ///< 6 s, 14 semitones, -72 .. 0 dB: the 2.0 view

/** X: the time both graphs show, and the distance of the time marks. */
struct TimeStep { float seconds, tickSeconds; };
/** Y: the pitch span, and the loudness span (72: the whole range -72 .. 0 dB). */
struct HeightStep { float semitones, levelSpanDb; };

constexpr TimeStep kTimeSteps[kZoomSteps] = { { 1.5f, 0.5f }, { 3.0f, 1.0f }, { 6.0f, 1.0f }, { 12.0f, 2.0f }, { 24.0f, 4.0f } };
constexpr HeightStep kHeightSteps[kZoomSteps] = { { 5.0f, 24.0f }, { 8.0f, 36.0f }, { 14.0f, 72.0f }, { 24.0f, 72.0f }, { 36.0f, 72.0f } };

/** The loudness lane's dB window: -72 .. 0 at a span of 72; narrower, it sits on the In and Out
    thresholds, both always inside with room. */
struct LevelWindow { float bottom = -72.0f, top = 0.0f; };
LevelWindow levelWindow (float spanDb, float gateInDb, float gateOutDb) noexcept;

/** dB between the marks of the label column: 6, 12 or 24, the smallest whose marks are at least
    16 px apart in a lane this high. */
int levelMarkStep (float spanDb, int laneHeightPx) noexcept;

} // namespace layout
