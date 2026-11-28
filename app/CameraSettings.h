// What the camera control remembers between runs.
#pragma once

#include <juce_core/juce_core.h>

#include "CameraBow.h"
#include "CameraLogic.h"
#include "CameraMarker.h"

struct CameraSettings
{
    /** Where an axis of the pad goes: a MIDI controller 0..127, or one of these. */
    enum Target { targetNone = -1, targetHarmonyMix = -2 };

    /** What a gesture does. */
    enum Action { actionNone = 0, actionHarmony, actionFreezeSolo, actionFreezeHarmony,
                  actionPresetNext, actionPresetPrevious, actionDetection, numActions };

    bool enabled = false;            ///< not stored: the camera is switched on by hand each time
    juce::String deviceName;         ///< empty = the first camera

    cam::PadTracker::Settings pad;
    int  xTarget = targetNone;       ///< sideways movement of the scroll
    int  yTarget = targetNone;       ///< up and down

    cam::GestureDetector::Settings gestures;
    int  nodAction = actionNone;
    int  shakeAction = actionNone;

    // --- 2.1 (keys padAnchor, markerOn, flickOn, flickSpeed, flickCooldownMs live in `pad`;
    // marker, markerTolerance, bowOn, bowThreshold, bowDebounceMs, bowDipDb, tab here)
    cam::MarkerColour marker;          ///< the sticker's colour (pipette); marker.valid false = none
    float markerTolerance = 1.0f;      ///< "Farbtoleranz", 0.5 .. 2, scales its tolerances
    bool  bowOn = false;               ///< "Bogenhand-Erkennung": the camera announces changes of bow
    cam::BowTracker::Settings bow;     ///< threshold, debounce (violinRightHand is filled from pad.rightHand)
    float bowDipDb = 4.5f;             ///< "Neuanschlag ab": the level dip that is a new stroke while announced, 3 .. 8
    int   tab = 0;                     ///< the window's last tab: 0 pad, 1 marker, 2 gestures, 3 bow hand

    /** The object as read: keys of other versions (1.5's heightTarget, low, high,
        smoothingMs, deadZone) are written back unchanged, so switching back keeps them. */
    juce::var stored;

    static bool validTarget (int t) noexcept { return t == targetNone || t == targetHarmonyMix || (t >= 0 && t <= 127); }

    /** The largest dead zone 2.0 knows, and so the largest written under its key "padDeadZone". */
    static constexpr float kLegacyMaxDeadZone = 0.15f;

    /** For the settings file. */
    juce::String toJson() const
    {
        auto* o = stored.getDynamicObject() != nullptr ? stored.getDynamicObject()->clone().release()
                                                       : new juce::DynamicObject();
        o->setProperty ("device", deviceName);
        o->setProperty ("rightHand", pad.rightHand);
        // 2.0 reads the dead zone clamped to 0.15. From 2.1 the instrument profile holds the
        // full value (up to 0.25); this key keeps what 2.0 can show, nothing more.
        o->setProperty ("padDeadZone", juce::jmin (pad.deadZone, kLegacyMaxDeadZone));
        o->setProperty ("padSpeed", pad.speed);
        o->setProperty ("responseX", pad.responseX);
        o->setProperty ("responseY", pad.responseY);
        o->setProperty ("xTarget", xTarget);
        o->setProperty ("yTarget", yTarget);
        o->setProperty ("sensitivity", gestures.sensitivity);
        o->setProperty ("cooldownMs", gestures.cooldownMs);
        o->setProperty ("nodAction", nodAction);
        o->setProperty ("shakeAction", shakeAction);
        return juce::JSON::toString (juce::var (o), true);
    }

    static CameraSettings fromJson (const juce::String& text)
    {
        CameraSettings s;
        const auto v = juce::JSON::parse (text);
        if (! v.isObject())
            return s;
        s.stored = v;

        const auto has = [&v] (const char* key)
        {
            const auto value = v.getProperty (key, juce::var());
            return value.isInt() || value.isInt64() || value.isDouble();
        };
        const auto number = [&v, &has] (const char* key, double fallback, double low, double high)
        {
            return has (key) ? juce::jlimit (low, high, (double) v.getProperty (key, juce::var())) : fallback;
        };
        const auto target = [&] (const char* key, int fallback)
        {
            const int t = (int) std::lround (number (key, fallback, -2.0, 127.0));
            return validTarget (t) ? t : fallback;
        };

        s.deviceName = v.getProperty ("device", juce::String()).toString();
        s.pad.rightHand = (bool) v.getProperty ("rightHand", false);
        s.pad.deadZone = (float) number ("padDeadZone", s.pad.deadZone, 0.03, kLegacyMaxDeadZone);   // read once, at start
        s.pad.speed = (float) number ("padSpeed", s.pad.speed, 0.1, 4.0);
        s.pad.responseX = (int) number ("responseX", s.pad.responseX, 0, cam::PadTracker::numResponses - 1);
        s.pad.responseY = (int) number ("responseY", s.pad.responseY, 0, cam::PadTracker::numResponses - 1);
        s.xTarget = target ("xTarget", s.xTarget);

        if (has ("yTarget"))
        {
            s.yTarget = target ("yTarget", s.yTarget);
        }
        else if (has ("heightTarget"))
        {
            // Until 1.5 the height of the violin went to: 0 nothing, 1 CC 1, 2 CC 11, 3 CC 74, 4 the harmony mix.
            // It is the up-and-down axis of the pad now.
            static const int kFromHeight[] = { targetNone, 1, 11, 74, targetHarmonyMix };
            s.yTarget = kFromHeight[(int) number ("heightTarget", 0, 0, 4)];
        }

        s.gestures.sensitivity = (float) number ("sensitivity", s.gestures.sensitivity, 0.0, 1.0);
        s.gestures.cooldownMs = (float) number ("cooldownMs", s.gestures.cooldownMs, 300.0, 5000.0);
        s.nodAction = (int) number ("nodAction", s.nodAction, 0, numActions - 1);
        s.shakeAction = (int) number ("shakeAction", s.shakeAction, 0, numActions - 1);
        return s;
    }
};
