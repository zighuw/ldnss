#pragma once

#include <vector>

struct MomentaryPoint {
    double seconds = 0.0;
    double lufs = 0.0;
};

struct LoudnessResult {
    double integratedLUFS = -120.0;
    double maxTruePeakDB = -120.0;
    double maxMomentaryLUFS = -120.0;
    double maxShortTermLUFS = -120.0;
    double loudnessRangeLU = 0.0;
    std::vector<MomentaryPoint> momentaryHistory;
    std::vector<MomentaryPoint> shortTermHistory;
    bool valid = false;
};
