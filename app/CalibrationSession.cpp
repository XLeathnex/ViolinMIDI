// One run of the automatic calibration (see CalibrationSession.h).
#include "CalibrationSession.h"

CalibrationSession::CalibrationSession (CalibrationHost& h) : host (h)
{
    strings = vm::calib::Instrument::fromLowestNote (host.savedEngine().lowestNote);
}

CalibrationSession::~CalibrationSession() = default;

void CalibrationSession::setMode (Mode m)
{
    chosenMode = m;
}

void CalibrationSession::setStrings (int count)
{
    strings = vm::calib::Instrument::withStrings (count);
}

bool CalibrationSession::stringsFromFactoryValue() const noexcept
{
    return false;
}

juce::String CalibrationSession::startProblem() const
{
    return {};
}

void CalibrationSession::start() {}

int CalibrationSession::currentTake() const noexcept
{
    return -1;
}

CalibrationSession::TakeState CalibrationSession::takeState (int) const noexcept
{
    return TakeState::open;
}

vm::calib::Progress CalibrationSession::progress() const
{
    return {};
}

juce::String CalibrationSession::hint() const
{
    return {};
}

bool CalibrationSession::canGoOn() const noexcept
{
    return false;
}

void CalibrationSession::next() {}
void CalibrationSession::repeatCurrent() {}
void CalibrationSession::skipCurrent() {}
void CalibrationSession::redo (int) {}

float CalibrationSession::analysisProgress() const noexcept
{
    return 0.0f;
}

int CalibrationSession::analysisPhase() const noexcept
{
    return 0;
}

const vm::calib::Result* CalibrationSession::result() const noexcept
{
    return nullptr;
}

void CalibrationSession::showSide (bool) {}

bool CalibrationSession::showingAfter() const noexcept
{
    return false;
}

bool CalibrationSession::save (bool)
{
    return false;
}

bool CalibrationSession::revert()
{
    return false;
}

bool CalibrationSession::isSaved() const noexcept
{
    return false;
}

void CalibrationSession::setKeepReport (bool) {}

bool CalibrationSession::keepReport() const noexcept
{
    return false;
}

juce::String CalibrationSession::reportFolder() const
{
    return {};
}

void CalibrationSession::abort()
{
    current = State::closed;
    if (onClose)
        onClose();
}

void CalibrationSession::close()
{
    abort();
}

void CalibrationSession::liveMeters (const AudioHost::Meters& m)
{
    lastMeters = m;
}

void CalibrationSession::cameraSample (double, bool, float, float, bool) {}

void CalibrationSession::showDemo (Demo) {}
