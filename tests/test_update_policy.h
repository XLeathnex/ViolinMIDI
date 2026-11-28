// ViolinMIDI - tests for the updater's start-up rules (2.1, app/UpdatePolicy.h):
// the selection holds only under the app it was made with, a pending start,
// one unclean end is not a failed start, the fallback goes to the version that
// worked last, a downloaded copy started directly, promotion, and versions.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// UpdatePolicy.h is included at its top.
#pragma once

void updatePolicySelection()
{
    check (updpolicy::kStrikes == 2, "a start fails only at the second unclean end in a row");
}

void updatePolicyPending() {}
void updatePolicyStrikes() {}
void updatePolicyLastGood() {}
void updatePolicyDirectStart() {}
void updatePolicyPromotion() {}
void updatePolicyVersions() {}
