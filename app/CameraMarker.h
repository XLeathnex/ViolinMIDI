// The colour marker on the violin scroll (2.1): the pipette that learns a
// sticker's colour from the preview, and the finder that looks for it in every
// camera frame. With the marker the pad follows the scroll itself instead of
// the violin hand.
//
// Plain C++ without JUCE or Apple frameworks, over a PixelView of 8-bit pixels
// (the camera's BGRA buffer or a copy of the RGB preview), so it runs in the
// tests. No allocation per frame: the finder's buffers are made in prepare().
#pragma once

#include "CameraLogic.h"

#include <cstdint>

namespace cam
{

/** 8-bit pixels: the camera's BGRA buffer or a copy of the RGB preview. */
struct PixelView
{
    const uint8_t* data = nullptr;
    int  width = 0, height = 0;
    int  rowBytes = 0;          ///< from one row to the next
    int  pixelBytes = 4;        ///< from one pixel to the next
    int  red = 2, green = 1, blue = 0;   ///< byte offsets inside a pixel (BGRA)
    bool mirrored = false;      ///< true for the camera buffer: picture x = 1 - buffer x (the preview is already mirrored)
};

struct Hsv { float h = 0.0f, s = 0.0f, v = 0.0f; };   ///< h 0 .. 360 degrees, s and v 0 .. 1

/** Integer max/min, the usual hexcone formula. */
Hsv toHsv (int r, int g, int b) noexcept;

/** A sticker's colour as the pipette took it. */
struct MarkerColour
{
    bool  valid = false;
    float hue = 0.0f, sat = 0.0f, val = 0.0f;                  ///< median over the sticker
    float hueTol = 14.0f, satTol = 0.25f, valTol = 0.4f;       ///< half widths around it
    float area = 0.0f;                                         ///< share of the picture it covered
    static constexpr float kMinSat = 0.30f, kMinVal = 0.18f;   ///< below: not a usable marker colour

    /** Circular hue distance; s and v also bounded below by kMinSat / kMinVal (scaled tolerances). */
    bool matches (const Hsv&, float toleranceScale = 1.0f) const noexcept;
};

struct Pick
{
    enum class Problem { none, nothing, pale, dark, common };
    Problem problem = Problem::nothing;   ///< none and common give a usable colour (common is a warning)
    MarkerColour colour;
    int   pixels = 0;                     ///< size of the sticker found, in pixels of the view
    float elsewhere = 0.0f;               ///< share of the rest of the picture with the same colour
};

/** The pipette: the sticker at (x, y), 0..1 in the picture as shown. No allocation. */
Pick pickColour (const PixelView&, float x, float y) noexcept;

/** Finds the sticker in camera frames. One per camera queue. */
class MarkerFinder
{
public:
    /** Work buffers for a grid of up to this many cells (call when the frame size changes). */
    void prepare (int maxCells);

    /** Every `step`-th pixel in both directions (2 for 640 x 480). Returns the marker's centre in
        picture coordinates (mirrored like Pose), confidence 0 if not found. */
    Point find (const PixelView&, int step, const MarkerColour&, float toleranceScale, double timeSec) noexcept;

    /** Forget where it was. */
    void reset() noexcept;
};

} // namespace cam
