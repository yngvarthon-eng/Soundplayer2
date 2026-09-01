#pragma once

#include <algorithm>
#include <cstdint>

namespace xtp
{

// Port of exTracker's Transport (extracker/src/transport.cpp), minus the
// wall-clock-driven background thread: exTracker advances one tick per sleep()
// on its own thread, but soundplayer2 pulls audio in fixed-size blocks, so here
// advanceTick() is called synchronously from XtpSequencerSource as it subdivides
// each block at tick boundaries. The tick-length formula, row/tick counters, and
// swing math are otherwise identical so playback timing matches exTracker exactly.
class TransportClock
{
public:
    void reset(int rowsInCurrentPattern)
    {
        patternRows = std::max(1, rowsInCurrentPattern);
        tickCount = 0;
        rowAdvanceCount = 0;
        currentRow = 0;
    }

    void setPatternRows(int rows) { patternRows = std::max(1, rows); }
    void setTempoBpm(double bpm)        { if (bpm > 0.0) tempoBpm = bpm; }
    void setTicksPerBeat(int tpb)       { if (tpb > 0) ticksPerBeat = tpb; }
    void setTicksPerRow(int tpr)        { if (tpr > 0) ticksPerRow = tpr; }
    void setSwingPercent(int percent)   { swingPercent = std::clamp(percent, 50, 75); }

    double tempoBpm_() const   { return tempoBpm; }
    int ticksPerBeat_() const  { return ticksPerBeat; }
    int ticksPerRow_() const   { return ticksPerRow; }
    std::uint64_t tick() const { return tickCount; }
    int row() const            { return currentRow; }

    // Seconds until the *next* tick, given the row the clock is on right now.
    // Matches Transport::runClock's per-tick formula exactly (including that the
    // swing factor is derived from the row at the moment the tick fires, not the
    // row it may advance into).
    double secondsUntilNextTick() const
    {
        const double bpm = std::max(1.0, tempoBpm);
        const int tpb = std::max(1, ticksPerBeat);
        double tickSeconds = 60.0 / (bpm * (double) tpb);

        const double swing = std::clamp(swingPercent, 50, 75) / 100.0;
        const double evenFactor = std::max(0.25, 2.0 * (1.0 - swing));
        const double oddFactor  = std::max(0.25, 2.0 * swing);
        const bool oddRow = (currentRow % 2) == 1;
        tickSeconds *= oddRow ? oddFactor : evenFactor;
        return tickSeconds;
    }

    // Advances by exactly one tick, updating currentRow when a row boundary is
    // crossed. Returns true if a new row was entered.
    bool advanceTick()
    {
        ++tickCount;
        const int tpr = std::max(1, ticksPerRow);
        if (tickCount % (std::uint64_t) tpr == 0)
        {
            ++rowAdvanceCount;
            currentRow = (int) (rowAdvanceCount % (std::uint64_t) patternRows);
            return true;
        }
        return false;
    }

    // ticksIntoRow = tickCount % ticksPerRow, using the *current* ticksPerRow --
    // if ticksPerRow changed since the last row boundary (effect 0x0F/E-speed),
    // this can differ from "ticks since row start" the same way exTracker's does.
    std::uint32_t ticksIntoRow() const
    {
        const int tpr = std::max(1, ticksPerRow);
        return (std::uint32_t) (tickCount % (std::uint64_t) tpr);
    }

    void jumpToRow(int row)
    {
        const int rows = std::max(1, patternRows);
        const int target = ((row % rows) + rows) % rows;
        const std::uint64_t tpr = (std::uint64_t) std::max(1, ticksPerRow);
        tickCount = (std::uint64_t) target * tpr;
        rowAdvanceCount = (std::uint64_t) target;
        currentRow = target;
    }

private:
    double tempoBpm = 125.0;
    int ticksPerBeat = 6;
    int ticksPerRow = 6;
    int patternRows = 64;
    int swingPercent = 50;

    std::uint64_t tickCount = 0;
    std::uint64_t rowAdvanceCount = 0;
    int currentRow = 0;
};

} // namespace xtp
