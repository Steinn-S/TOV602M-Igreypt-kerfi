#include "skotturns_stjornun.hpp"

#include <algorithm>
#include <cmath>

// Þegar ég tala um smooth er ég að reyna gera hreyfingu búnaðarsins meira fluid og koma í veg fyrir of hraðarbreytingar

std::pair<int, int> SkotturnsStjornun::computePanTilt(int face_cx, int face_cy,
                                                       int frame_cx, int frame_cy) {
    // Er að gera breytingar á miðju skotmarksins smá smooth til að koma í veg fyrir allt of hraðar breytingar
    if (!smooth_face_x_.has_value()) {
        smooth_face_x_ = static_cast<double>(face_cx);
        smooth_face_y_ = static_cast<double>(face_cy);
    } else {
        *smooth_face_x_ += FACE_SMOOTHING * (face_cx - *smooth_face_x_);
        *smooth_face_y_ += FACE_SMOOTHING * (face_cy - *smooth_face_y_);
    }

    // Reiknar skekkju frá myndmiðju
    const int error_x =  static_cast<int>(*smooth_face_x_) - frame_cx;
    const int error_y =  static_cast<int>(*smooth_face_y_) - frame_cy;

    const double pan_target  = static_cast<double>(axis_rate( error_x, DEADBAND_X, PAN_GAIN,  SLOW_ZONE_X));
    const double tilt_target = static_cast<double>(axis_rate(-error_y, DEADBAND_Y, TILT_GAIN, SLOW_ZONE_Y));

    // Gerir hraðan meira smooth
    smooth_pan_  += RATE_SMOOTHING * (pan_target  - smooth_pan_);
    smooth_tilt_ += RATE_SMOOTHING * (tilt_target - smooth_tilt_);

    return {static_cast<int>(smooth_pan_), static_cast<int>(smooth_tilt_)};
}

std::pair<int, int> SkotturnsStjornun::decay() {
    // Dregur hraða niður þegar skotmark sést ekki.
    smooth_pan_  *= (1.0 - RATE_SMOOTHING);
    smooth_tilt_ *= (1.0 - RATE_SMOOTHING);
    smooth_face_x_.reset();
    smooth_face_y_.reset();

    return {static_cast<int>(smooth_pan_), static_cast<int>(smooth_tilt_)};
}

void SkotturnsStjornun::reset() {
    smooth_face_x_.reset();
    smooth_face_y_.reset();
    smooth_pan_  = 0.0;
    smooth_tilt_ = 0.0;
}

// Hjálparföll

int SkotturnsStjornun::axis_rate(int error, int deadband, double gain, int slow_zone) const {
    if (std::abs(error) <= deadband)
        return 0;

    const int adjusted = std::abs(error) - deadband;
    double rate = adjusted * gain;

    if (adjusted <= slow_zone)
        rate *= SLOW_ZONE_SCALE;

    const int clamped = std::max(0, std::min(static_cast<int>(rate), MAX_RATE));
    return error > 0 ? clamped : -clamped;
}
