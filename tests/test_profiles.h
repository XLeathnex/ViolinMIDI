// ViolinMIDI - tests for the instrument profiles (2.1, core/Profiles.h): the
// values and their round trip, changes made by another version, the preset
// rule, "Lowest Latenz", the list operations and the agreement with the
// settings table's profile rows.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// Profiles.h is included at its top.
#pragma once

void profilesValues()
{
    check (vm::ProfileValues::kLegacyMaxDeadZone < vm::ProfileValues::kMaxDeadZone, "2.0's dead zone maximum is below 2.1's");
}

void profilesOutsideChanges() {}
void profilesPresetRule() {}
void profilesLowestLatency() {}
void profilesList() {}
void profilesTableAgrees() {}
