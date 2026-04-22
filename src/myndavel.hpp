#pragma once

#include <string>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

// Uppsetning myndavélar
class Myndavel {
public:
    explicit Myndavel(int width        = 320,
                      int height       = 240,
                      int camera_index = 0,
                      bool use_mjpeg  = true,
                      int  target_fps = 30,
                      bool rotate_180 = false);
    ~Myndavel();

    Myndavel(const Myndavel&)            = delete;
    Myndavel& operator=(const Myndavel&) = delete;

    void open();
    void close();
    bool is_open() const;

    cv::Mat captureFrame();
    void    setMode(const std::string& mode);
    void    autofocus();

    int width()  const { return width_;  }
    int height() const { return height_; }

private:
    int               width_;
    int               height_;
    int               camera_index_;
    bool              use_mjpeg_;
    int               target_fps_;
    bool              rotate_180_;
    cv::VideoCapture  cap_;
};
