#include "myndavel.hpp"

#include <stdexcept>

#include <opencv2/imgproc.hpp>

Myndavel::Myndavel(int width, int height, int camera_index,
                   bool use_mjpeg, int target_fps, bool rotate_180)
    : width_(width),
      height_(height),
      camera_index_(camera_index),
      use_mjpeg_(use_mjpeg),
      target_fps_(target_fps),
      rotate_180_(rotate_180) {}

Myndavel::~Myndavel() { close(); }

void Myndavel::open() {
    cap_.open(camera_index_);
    if (!cap_.isOpened())
        throw std::runtime_error("Myndavel: cannot open camera index " +
                                 std::to_string(camera_index_));

    if (use_mjpeg_)
        cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap_.set(cv::CAP_PROP_FPS,          target_fps_);
}

void Myndavel::close() {
    if (cap_.isOpened())
        cap_.release();
}

bool Myndavel::is_open() const { return cap_.isOpened(); }


cv::Mat Myndavel::captureFrame() {
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty())
        throw std::runtime_error("Myndavel: failed to read frame");

    if (rotate_180_)
        cv::rotate(frame, frame, cv::ROTATE_180);

    return frame;
}

void Myndavel::setMode(const std::string& mode) {
    // Hægt er að bæta við fleiri myndavélastillingum, notaði þetta meira þegar ég var að prófa að fá betra framerate en það virkaði ekki almennilega
    (void)mode;
}

void Myndavel::autofocus() {
    // Reynir að kveikja á autofocus ef myndavélin styður það
    cap_.set(cv::CAP_PROP_AUTOFOCUS, 1);
}
