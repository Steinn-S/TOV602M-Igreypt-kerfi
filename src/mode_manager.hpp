#pragma once

#include <chrono>
#include <optional>
#include <utility>

enum class Mode { TRACKING, SEARCH, IDLE };

// Heldur utan um ham kerfisins
class ModeManager {
public:
    ModeManager();

    void setMode(Mode mode);
    Mode getMode() const;

    // Kallað þegar skotmark sést
    void targetSeen();

    // Kallað þegar skotmark sést ekki
    std::optional<std::pair<int, int>> handleLostTarget();

private:
    Mode mode_;
    std::chrono::steady_clock::time_point last_seen_;
    std::chrono::steady_clock::time_point last_search_move_;
    int pan_dir_;
    int tilt_level_;
    int pan_sweeps_;

    // Tími án skotmarks áður en leit byrjar
    static constexpr double SEARCH_TRIGGER_S  = 2.2;
    // Minnsta bil milli leitarhreyfinga
    static constexpr double SEARCH_UPDATE_S   = 0.7;
    // Skref í pan leit
    static constexpr int    SEARCH_STEP_PAN   = 45;
    // Skref í tilt leit
    static constexpr int    SEARCH_STEP_TILT  = 12;
    // Hámark á fjölda tilt hreyfinga 
    static constexpr int    SEARCH_TILT_LIMIT = 3;
};
