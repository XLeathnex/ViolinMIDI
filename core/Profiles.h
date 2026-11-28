// ViolinMIDI core - instrument profiles (2.1).
//
// A profile is what belongs to one violin with its pickup: the 13 detection
// values (input gain, In and Out threshold, both octave guards, the jump guard,
// the bridge, the lowest note) and the camera dead zone. Presets no longer
// change them (vm::keepOnPresetLoad), the calibration sets them for the active
// profile, and there may be any number of profiles. This file holds the values
// and the list; app/ProfileStore keeps them in the settings file.
//
// Plain C++17 without dependencies, like the rest of core/.
#pragma once

#include "SettingsText.h"
#include "Types.h"

#include <string>
#include <vector>

namespace vm
{

/** What belongs to one violin with its pickup: the detection values and the camera dead zone. */
struct ProfileValues
{
    float inputGainDb = 0.0f, gateDb = -45.0f, gateOffDb = -51.0f;
    bool  octaveAttackGuard = true;  float octaveAttackMs = 50.0f;
    bool  octaveSustainGuard = true; float octaveSustainMs = 11.0f;
    bool  jumpGuard = false; int jumpIntervalSemis = 7; float jumpWindowMs = 80.0f, jumpLockMs = 50.0f;
    float bridgeMs = 30.0f;
    int   lowestNote = 34;
    float deadZone = 0.06f;            ///< CameraSettings::pad.deadZone

    static constexpr float kMinDeadZone = 0.03f, kMaxDeadZone = 0.25f;
    static constexpr float kLegacyMaxDeadZone = 0.15f;   ///< = CameraSettings::kLegacyMaxDeadZone: 2.0 clamps to this

    static ProfileValues of (const Settings&, float deadZone) noexcept;
    void applyTo (Settings&) const noexcept;             ///< the 13 engine fields; the dead zone is the caller's
    ProfileValues clamped() const noexcept;              ///< the ranges of the settings table, Out <= In
};

/** Key (settings file, presets, profile JSON) and German label of each value, in the order of
    ProfileValues; the first 13 are the table rows of Scope::profile, the last is "padDeadZone". */
struct ProfileField { const char* key; const char* label; };
constexpr int kProfileFieldCount = 14;
extern const ProfileField kProfileFields[kProfileFieldCount];

/** Floats within 1e-4. */
bool sameProfileValues (const ProfileValues&, const ProfileValues&) noexcept;

/** Indices into kProfileFields where `outside` (the plain keys) differs from `profile`. A dead zone
    that is just the profile's clamped to 2.0's maximum does not count. */
std::vector<int> outsideChanges (const ProfileValues& outside, const ProfileValues& profile);

/** The preset rule of 2.1, under its profile name: = vm::keepOnPresetLoad (the profile fields
    and the wiring keep their live values). */
inline void keepProfile (Settings& loaded, const Settings& live)   { keepOnPresetLoad (loaded, live); }

/** The factory preset "Lowest Latenz": Ansprache Schnell, Bewegung Stufenlos, both smoothings 0,
    overlap 0, release 0. It touches no profile field (2.1). The buffer is the app's part. */
void applyLowestLatency (Settings&) noexcept;

struct InstrumentProfile
{
    int id = 0;
    std::string name;        ///< UTF-8
    ProfileValues values;
};

/** The profiles in creation order, with the active one. Ids are never used again. */
class ProfileList
{
public:
    bool empty() const noexcept                                  { return list.empty(); }
    const std::vector<InstrumentProfile>& all() const noexcept  { return list; }
    const InstrumentProfile* find (int id) const noexcept;
    int  activeId() const noexcept                               { return active; }   ///< 0 only while empty
    const InstrumentProfile* activeProfile() const noexcept     { return find (active); }
    bool setActive (int id) noexcept;
    /** The name is made unique; the first profile becomes active. Returns the new id. */
    int  add (const std::string& name, const ProfileValues&);
    /** Unique among the others. */
    bool rename (int id, const std::string& name);
    /** Refused (0) for the last one. Removing the active one activates the next (or the one
        before). Returns the active id. */
    int  remove (int id);
    /** Clamped; true if anything changed. */
    bool updateActive (const ProfileValues&);
    /** "X", "X 2", "X 3" ... (ASCII case-insensitive). */
    std::string uniqueName (const std::string& wanted, int exceptId = 0) const;
    /** Next / previous, wrapping. */
    int  step (int fromId, int delta) const noexcept;
    /** From disk: duplicate or invalid ids get new ones, empty names "Profil", an unknown active
        id the first profile. */
    void restore (std::vector<InstrumentProfile> profiles, int activeId, int nextId);
    int  nextId() const noexcept                                 { return counter; }

private:
    std::vector<InstrumentProfile> list;
    int active = 0, counter = 1;
};

} // namespace vm
