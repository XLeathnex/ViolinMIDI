// What the camera control remembers between runs.
#pragma once

#include <juce_core/juce_core.h>

#include "CameraLogic.h"

struct CameraSettings
{
    /** Where the height of the violin goes. */
    enum Target { targetNone = 0, targetModWheel, targetExpression, targetFilter, targetHarmonyLevel, numTargets };

    /** What a gesture does. */
    enum Action { actionNone = 0, actionHarmony, actionFreezeSolo, actionFreezeHarmony,
                  actionPresetNext, actionPresetPrevious, actionDetection, numActions };

    bool enabled = false;            ///< not stored: the camera is switched on by hand each time
    juce::String deviceName;         ///< empty = the first camera

    cam::HeightTracker::Settings height;
    int  heightTarget = targetNone;

    cam::GestureDetector::Settings gestures;
    int  nodAction = actionNone;
    int  shakeAction = actionNone;

    /** For the settings file. */
    juce::String toJson() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("device", deviceName);
        o->setProperty ("rightHand", height.rightHand);
        o->setProperty ("low", height.low);
        o->setProperty ("high", height.high);
        o->setProperty ("smoothingMs", height.smoothingMs);
        o->setProperty ("deadZone", height.deadZone);
        o->setProperty ("heightTarget", heightTarget);
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

        const auto number = [&v] (const char* key, double fallback, double low, double high)
        {
            const auto value = v.getProperty (key, juce::var());
            return value.isInt() || value.isInt64() || value.isDouble() ? juce::jlimit (low, high, (double) value) : fallback;
        };

        s.deviceName = v.getProperty ("device", juce::String()).toString();
        s.height.rightHand = (bool) v.getProperty ("rightHand", false);
        s.height.low = (float) number ("low", s.height.low, -5.0, 5.0);
        s.height.high = (float) number ("high", s.height.high, -5.0, 5.0);
        s.height.smoothingMs = (float) number ("smoothingMs", s.height.smoothingMs, 0.0, 1000.0);
        s.height.deadZone = (float) number ("deadZone", s.height.deadZone, 0.0, 0.2);
        s.heightTarget = (int) number ("heightTarget", s.heightTarget, 0, numTargets - 1);
        s.gestures.sensitivity = (float) number ("sensitivity", s.gestures.sensitivity, 0.0, 1.0);
        s.gestures.cooldownMs = (float) number ("cooldownMs", s.gestures.cooldownMs, 300.0, 5000.0);
        s.nodAction = (int) number ("nodAction", s.nodAction, 0, numActions - 1);
        s.shakeAction = (int) number ("shakeAction", s.shakeAction, 0, numActions - 1);
        return s;
    }
};
