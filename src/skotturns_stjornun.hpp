#pragma once

#include <optional>
#include <utility>

// Reiknar PAN og TILT hraða út frá staðsetningu skotmarks
class SkotturnsStjornun {
public:
    SkotturnsStjornun() = default;

    // Tökum miðju skotmarksins og miðju myndarinnar og skilar PAN og TILT hraða sem þarf til að færa skotmarkið í miðjuna
    std::pair<int, int> computePanTilt(int face_cx, int face_cy,
                                       int frame_cx, int frame_cy);

    // Kallað þegar skotmark sést ekki
    std::pair<int, int> decay();

    void reset();

private:
    int axis_rate(int error, int deadband, double gain, int slow_zone) const;

    std::optional<double> smooth_face_x_;
    std::optional<double> smooth_face_y_;
    double smooth_pan_  = 0.0;
    double smooth_tilt_ = 0.0;

    // Breytir myndskekkju í hraða
    static constexpr double PAN_GAIN        = 2.8;
    static constexpr double TILT_GAIN       = 2.1;
    static constexpr int    MAX_RATE        = 320;   // Hámarkshraði á ás

    // Svæði þar sem engin hreyfing er
    static constexpr int    DEADBAND_X      = 20;    // Pixlar
    static constexpr int    DEADBAND_Y      = 16;

    // Hægara svæði nær miðju
    static constexpr int    SLOW_ZONE_X     = 140;   // Pixlar
    static constexpr int    SLOW_ZONE_Y     = 110;
    static constexpr double SLOW_ZONE_SCALE = 0.35;

    // Sléttun
    static constexpr double FACE_SMOOTHING  = 0.55;
    static constexpr double RATE_SMOOTHING  = 0.35;
};
