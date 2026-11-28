// Start-up, fallback and promotion rules of the updater (see UpdatePolicy.h).
#include "UpdatePolicy.h"

namespace updpolicy
{

int compareVersions (const std::string&, const std::string&)
{
    return 0;
}

Decision onInstalledStart (const Facts& facts)
{
    Decision d;
    d.state = facts.state;
    d.history = facts.history;
    return d;
}

Decision onDownloadedStart (const Facts& facts)
{
    Decision d;
    d.state = facts.state;
    d.history = facts.history;
    return d;
}

Promotion promotion (const PromotionFacts&)
{
    return Promotion::notDue;
}

} // namespace updpolicy
