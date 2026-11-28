// ViolinMIDI core - instrument profiles (see Profiles.h).
#include "Profiles.h"

#include <algorithm>
#include <cmath>

namespace vm
{

const ProfileField kProfileFields[kProfileFieldCount] = {
    { "gainDb",             "Verst\xc3\xa4rkung" },
    { "gateDb",             "In-Schwelle" },
    { "gateOffDb",          "Out-Schwelle" },
    { "octaveAttackGuard",  "Anstreich-Schutz" },
    { "octaveAttackMs",     "Anstreich-Schutz (ms)" },
    { "octaveSustainGuard", "Oktavsprung-Sperre" },
    { "octaveSustainMs",    "Oktavsprung-Sperre (ms)" },
    { "jumpGuard",          "Sprungschutz" },
    { "jumpIntervalSemis",  "Sprungschutz ab" },
    { "jumpWindowMs",       "Sprungschutz-Fenster" },
    { "jumpLockMs",         "Sprungschutz-Sperrzeit" },
    { "bridgeMs",           "\xc3\x9c" "berbr\xc3\xbc" "ckung" },
    { "lowestNote",         "Tiefster Ton" },
    { "padDeadZone",        "Kamera-Totzone" }
};

namespace
{
    template <typename T> T clampTo (T v, T lo, T hi) noexcept { return std::min (hi, std::max (lo, v)); }
    bool near (float a, float b) noexcept { return std::abs (a - b) < 1.0e-4f; }

    /** Field i of the two values differs. */
    bool differs (const ProfileValues& a, const ProfileValues& b, int i) noexcept
    {
        switch (i)
        {
            case 0:  return ! near (a.inputGainDb, b.inputGainDb);
            case 1:  return ! near (a.gateDb, b.gateDb);
            case 2:  return ! near (a.gateOffDb, b.gateOffDb);
            case 3:  return a.octaveAttackGuard != b.octaveAttackGuard;
            case 4:  return ! near (a.octaveAttackMs, b.octaveAttackMs);
            case 5:  return a.octaveSustainGuard != b.octaveSustainGuard;
            case 6:  return ! near (a.octaveSustainMs, b.octaveSustainMs);
            case 7:  return a.jumpGuard != b.jumpGuard;
            case 8:  return a.jumpIntervalSemis != b.jumpIntervalSemis;
            case 9:  return ! near (a.jumpWindowMs, b.jumpWindowMs);
            case 10: return ! near (a.jumpLockMs, b.jumpLockMs);
            case 11: return ! near (a.bridgeMs, b.bridgeMs);
            case 12: return a.lowestNote != b.lowestNote;
            case 13: return ! near (a.deadZone, b.deadZone);
            default: return false;
        }
    }
}

ProfileValues ProfileValues::of (const Settings& s, float zone) noexcept
{
    ProfileValues v;
    v.inputGainDb = s.inputGainDb;               v.gateDb = s.gateDb;             v.gateOffDb = s.gateOffDb;
    v.octaveAttackGuard = s.octaveAttackGuard;   v.octaveAttackMs = s.octaveAttackMs;
    v.octaveSustainGuard = s.octaveSustainGuard; v.octaveSustainMs = s.octaveSustainMs;
    v.jumpGuard = s.jumpGuard;                   v.jumpIntervalSemis = s.jumpIntervalSemis;
    v.jumpWindowMs = s.jumpWindowMs;             v.jumpLockMs = s.jumpLockMs;     v.bridgeMs = s.bridgeMs;
    v.lowestNote = s.lowestNote;                 v.deadZone = zone;
    return v;
}

void ProfileValues::applyTo (Settings& s) const noexcept
{
    s.inputGainDb = inputGainDb;                 s.gateDb = gateDb;               s.gateOffDb = gateOffDb;
    s.octaveAttackGuard = octaveAttackGuard;     s.octaveAttackMs = octaveAttackMs;
    s.octaveSustainGuard = octaveSustainGuard;   s.octaveSustainMs = octaveSustainMs;
    s.jumpGuard = jumpGuard;                     s.jumpIntervalSemis = jumpIntervalSemis;
    s.jumpWindowMs = jumpWindowMs;               s.jumpLockMs = jumpLockMs;       s.bridgeMs = bridgeMs;
    s.lowestNote = lowestNote;
}

ProfileValues ProfileValues::clamped() const noexcept
{
    ProfileValues v (*this);
    v.inputGainDb = clampTo (v.inputGainDb, -12.0f, 36.0f);
    v.gateDb = clampTo (v.gateDb, -70.0f, -20.0f);
    v.gateOffDb = std::min (clampTo (v.gateOffDb, -90.0f, -20.0f), v.gateDb);
    v.octaveAttackMs = clampTo (v.octaveAttackMs, 0.0f, 60.0f);
    v.octaveSustainMs = clampTo (v.octaveSustainMs, 0.0f, 100.0f);
    v.jumpIntervalSemis = clampTo (v.jumpIntervalSemis, 5, 24);
    v.jumpWindowMs = clampTo (v.jumpWindowMs, 10.0f, 200.0f);
    v.jumpLockMs = clampTo (v.jumpLockMs, 20.0f, 150.0f);
    v.bridgeMs = clampTo (v.bridgeMs, 0.0f, 200.0f);
    v.lowestNote = clampTo (v.lowestNote, kAbsLowestNote, 60);
    v.deadZone = clampTo (v.deadZone, kMinDeadZone, kMaxDeadZone);
    return v;
}

bool sameProfileValues (const ProfileValues& a, const ProfileValues& b) noexcept
{
    for (int i = 0; i < kProfileFieldCount; ++i)
        if (differs (a, b, i))
            return false;
    return true;
}

std::vector<int> outsideChanges (const ProfileValues&, const ProfileValues&)
{
    return {};
}

void applyLowestLatency (Settings&) noexcept {}

//==============================================================================
const InstrumentProfile* ProfileList::find (int) const noexcept
{
    return nullptr;
}

bool ProfileList::setActive (int) noexcept
{
    return false;
}

int ProfileList::add (const std::string&, const ProfileValues&)
{
    return 0;
}

bool ProfileList::rename (int, const std::string&)
{
    return false;
}

int ProfileList::remove (int)
{
    return active;
}

bool ProfileList::updateActive (const ProfileValues&)
{
    return false;
}

std::string ProfileList::uniqueName (const std::string& wanted, int) const
{
    return wanted;
}

int ProfileList::step (int fromId, int) const noexcept
{
    return fromId;
}

void ProfileList::restore (std::vector<InstrumentProfile>, int, int) {}

} // namespace vm
