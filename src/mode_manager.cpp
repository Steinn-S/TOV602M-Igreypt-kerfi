#include "mode_manager.hpp"

using Clock = std::chrono::steady_clock;

ModeManager::ModeManager()
    : mode_(Mode::TRACKING),
      last_seen_(Clock::now()),
      last_search_move_(Clock::now()),
      pan_dir_(1),
      tilt_level_(0),
      pan_sweeps_(0) {}


void ModeManager::setMode(Mode mode) {
    mode_ = mode;
}

Mode ModeManager::getMode() const {
    return mode_;
}

void ModeManager::targetSeen() {
    last_seen_ = Clock::now();
    if (mode_ == Mode::SEARCH) {
        mode_       = Mode::TRACKING;
        pan_sweeps_ = 0;
        tilt_level_ = 0;
    }
}

std::optional<std::pair<int, int>> ModeManager::handleLostTarget() {
    const auto now     = Clock::now();
    const double lost_s = std::chrono::duration<double>(now - last_seen_).count();

    if (lost_s < SEARCH_TRIGGER_S)
        return std::nullopt;  // // Ekki hefja leit fyrr en biðtíminn er liðinn


    mode_ = Mode::SEARCH;

    const double since_last_move =
        std::chrono::duration<double>(now - last_search_move_).count();

    if (since_last_move < SEARCH_UPDATE_S)
        return std::nullopt;  // Ekki kominn tími á næstu leit

    int pan_move  = SEARCH_STEP_PAN * pan_dir_;
    int tilt_move = 0;

    // Eftir tvær pan hreyfingar færist tilt niður
    if (pan_dir_ > 0) {
        ++pan_sweeps_;
        if (pan_sweeps_ >= 2 && tilt_level_ < SEARCH_TILT_LIMIT) {
            ++tilt_level_;
            tilt_move = -SEARCH_STEP_TILT;
        }
    }

    pan_dir_         = -pan_dir_;
    last_search_move_ = now;
    return std::make_pair(pan_move, tilt_move);
}
