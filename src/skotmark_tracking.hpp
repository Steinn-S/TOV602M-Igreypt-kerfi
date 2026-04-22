#pragma once

#include <filesystem>
#include <optional>

#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>
#include <opencv2/tracking.hpp>

// Finnur og rekur skotmarkið
class SkotmarkTracking {
public:
    SkotmarkTracking(int width, int height, const std::filesystem::path& model_path);

    // Aðgerðir úr UML.
    std::optional<cv::Rect> detectTarget(const cv::Mat& frame);
    cv::Point               centroid(const cv::Rect& box) const;

    void reset();

    // Finnur smá brasa að tengja við þannig þetta finnur YuNet á algengum stöðum
    static std::filesystem::path findModel(const std::filesystem::path& hint = {});

private:
    std::optional<cv::Rect> run_yunet(const cv::Mat& frame);
    void                    init_tracker(const cv::Mat& frame, const cv::Rect& box);

    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Ptr<cv::Tracker>        tracker_;
    int                         fail_count_  = 0;

    int width_;
    int height_;

    static constexpr double SCORE_THRESHOLD  = 0.75;
    static constexpr double NMS_THRESHOLD    = 0.3;
    static constexpr int    TOP_K            = 1;
    static constexpr int    FAIL_LIMIT       = 8;
};
