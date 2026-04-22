#include "skotmark_tracking.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace {

static const char* const MODEL_NAME = "face_detection_yunet_2023mar.onnx";

int clamp(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // Þetta er 

SkotmarkTracking::SkotmarkTracking(int width, int height,
                                   const std::filesystem::path& model_path)
    : width_(width), height_(height) {
    detector_ = cv::FaceDetectorYN::create(
        model_path.string(), "",
        cv::Size(width_, height_),
        SCORE_THRESHOLD, NMS_THRESHOLD, TOP_K);
}


std::optional<cv::Rect> SkotmarkTracking::detectTarget(const cv::Mat& frame) {
    // Reynir fyrst að nota tracker og ef það bregst er YuNet keyrt aftur
    if (tracker_) {
        cv::Rect tracked;
        const bool ok = tracker_->update(frame, tracked);
        if (ok) {
            fail_count_ = 0;
            return tracked;
        }
        if (++fail_count_ >= FAIL_LIMIT)
            reset();
    }

    // Ef tracker bregst er YuNet keyrt aftur
    const auto box = run_yunet(frame);
    if (box.has_value()) {
        init_tracker(frame, *box);
        fail_count_ = 0;
    }
    return box;
}

cv::Point SkotmarkTracking::centroid(const cv::Rect& box) const {
    return {box.x + box.width / 2, box.y + box.height / 2};
}

void SkotmarkTracking::reset() {
    tracker_    = nullptr;
    fail_count_ = 0;
}

// Hjálparfall fyrir að finna YuNet líkanið

std::filesystem::path SkotmarkTracking::findModel(const std::filesystem::path& hint) {
    std::vector<std::filesystem::path> candidates;

    if (!hint.empty())
        candidates.push_back(hint);

    candidates.push_back(std::filesystem::current_path() / "models" / MODEL_NAME);
    candidates.push_back(std::filesystem::current_path() / ".." / "models" / MODEL_NAME);
    candidates.push_back(std::filesystem::current_path() / ".." /
                         "Ígreypt kóði" / "tracker" / "models" / MODEL_NAME);
    candidates.push_back(std::filesystem::current_path() / ".." /
                         "Ígreypt kóði" / "C++ version" / "models" / MODEL_NAME);
    candidates.push_back(std::filesystem::path("/usr/share/opencv4") / MODEL_NAME);
    candidates.push_back(std::filesystem::path("/usr/local/share/opencv4") / MODEL_NAME);

    for (const auto& path : candidates) {
        const auto canonical = std::filesystem::weakly_canonical(path);
        if (std::filesystem::exists(canonical))
            return canonical;
    }

    throw std::runtime_error(
        std::string("SkotmarkTracking: cannot find YuNet model '") +
        MODEL_NAME + "'. Set YUNET_MODEL_PATH or place the file in models/");
}

// Innri hjálparföll

std::optional<cv::Rect> SkotmarkTracking::run_yunet(const cv::Mat& frame) {
    detector_->setInputSize(frame.size());

    cv::Mat faces;
    detector_->detect(frame, faces);
    if (faces.empty())
        return std::nullopt;

    // Velur stærsta skotmarkið.
    int   best  = 0;
    float bestA = -1.0f;
    for (int r = 0; r < faces.rows; ++r) {
        const float* f = faces.ptr<float>(r);
        const float  a = f[2] * f[3];
        if (a > bestA) { bestA = a; best = r; }
    }

    const float* f = faces.ptr<float>(best);
    const int x = clamp(static_cast<int>(f[0]), 0, frame.cols - 1);
    const int y = clamp(static_cast<int>(f[1]), 0, frame.rows - 1);
    const int w = clamp(static_cast<int>(f[2]), 1, frame.cols - x);
    const int h = clamp(static_cast<int>(f[3]), 1, frame.rows - y);
    return cv::Rect(x, y, w, h);
}

void SkotmarkTracking::init_tracker(const cv::Mat& frame, const cv::Rect& box) {
    tracker_ = cv::TrackerKCF::create();
    tracker_->init(frame, box);
}
