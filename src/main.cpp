// Aðalforrit sem tengir allar einingar saman.

#include "mode_manager.hpp"
#include "myndavel.hpp"
#include "skotmark_tracking.hpp"
#include "skotturns_stjornun.hpp"
#include "uart_link.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

// Stillingar

namespace {

static constexpr int FRAME_WIDTH  = 320;
static constexpr int FRAME_HEIGHT = 240;
static constexpr int FRAME_CX     = FRAME_WIDTH  / 2;
static constexpr int FRAME_CY     = FRAME_HEIGHT / 2;

// Hve oft RATE skipun er send.
static constexpr double UPDATE_PERIOD_S = 0.06;

static const char* const TRACKING_MICROSTEP = "HALF";
static const char* const TRACKING_PROFILE   = "TORQUE";

struct Config {
    std::string pico_port   = "/dev/serial0";   // UART tenging 
    int         pico_baud   = 115200;
    int         camera_idx  = 0;
    bool        use_mjpeg   = true;
    int         target_fps  = 30;
    bool        rotate_180  = false;
    bool        show_window = false;
    bool        show_fps    = true;
    bool        verbose     = false;
    std::filesystem::path model_path;
};

std::string env_str(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return v ? v : fallback;
}

int env_int(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return v ? std::stoi(v) : fallback;
}

bool env_bool(const char* name, bool fallback) {
    const char* v = std::getenv(name);
    return v ? (std::string(v) != "0") : fallback;
}

Config load_config() {
    Config c;
    c.pico_port   = env_str("PICO_PORT",                    c.pico_port.c_str());
    c.pico_baud   = env_int("PICO_BAUD",                    c.pico_baud);
    c.camera_idx  = env_int("FACE_TRACK_USB_CAMERA_INDEX",  c.camera_idx);
    c.use_mjpeg   = env_bool("FACE_TRACK_USB_USE_MJPEG",    c.use_mjpeg);
    c.target_fps  = env_int("FACE_TRACK_USB_TARGET_FPS",    c.target_fps);
    c.rotate_180  = env_bool("FACE_TRACK_ROTATE_180",       c.rotate_180);
    c.show_window = env_bool("FACE_TRACK_SHOW_OPENCV_WINDOW", c.show_window);
    c.show_fps    = env_bool("FACE_TRACK_SHOW_FPS",         c.show_fps);
    c.verbose     = env_bool("FACE_TRACK_VERBOSE_PICO",     c.verbose);

    if (const char* v = std::getenv("YUNET_MODEL_PATH"))
        c.model_path = v;

    return c;
}

// Merki til að stöðva aðallykkjuna
std::atomic<bool> running{true};

void on_signal(int) { running.store(false); }

void log_pico(const char* prefix, const std::vector<std::string>& lines, bool verbose) {
    const bool has_err = std::any_of(lines.begin(), lines.end(),
        [](const std::string& l){ return l.rfind("ERR", 0) == 0; });

    if (!verbose && !has_err) return;

    std::cout << prefix;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) std::cout << " | ";
        std::cout << lines[i];
    }
    std::cout << '\n';
}

} 

// Aðalfallið

int main(int /*argc*/, char** /*argv*/) {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    const Config cfg = load_config();

    UartLink          uart    (cfg.pico_port, cfg.pico_baud);
    Myndavel          camera  (FRAME_WIDTH, FRAME_HEIGHT,
                               cfg.camera_idx, cfg.use_mjpeg,
                               cfg.target_fps, cfg.rotate_180);
    SkotturnsStjornun steering;
    ModeManager       modes;

    // Finnur YuNet
    const auto model_path = SkotmarkTracking::findModel(cfg.model_path);
    SkotmarkTracking tracker(FRAME_WIDTH, FRAME_HEIGHT, model_path);

    auto cleanup = [&]() {
        if (uart.is_open()) {
            try {
                log_pico("PICO: ", uart.stop(),                  cfg.verbose);
                log_pico("PICO: ", uart.sendCommand("HOLD 0"),   cfg.verbose);
                log_pico("PICO: ", uart.enable(false),           cfg.verbose);
            } catch (...) {}
            uart.close();
        }
        camera.close();
        cv::destroyAllWindows();
    };

    try {
        // Tengist Pico og stillir grunnstillingar
        uart.connect();
        log_pico("PICO: ", uart.sendCommand(std::string("MICROSTEP PAN ")  + TRACKING_MICROSTEP), cfg.verbose);
        log_pico("PICO: ", uart.sendCommand(std::string("MICROSTEP TILT ") + TRACKING_MICROSTEP), cfg.verbose);
        log_pico("PICO: ", uart.sendCommand(std::string("PROFILE ")         + TRACKING_PROFILE),  cfg.verbose);
        log_pico("PICO: ", uart.sendCommand("HOLD 1"),                                             cfg.verbose);
        log_pico("PICO: ", uart.enable(true),                                                      cfg.verbose);

        camera.open();

        auto last_update    = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        auto fps_window     = std::chrono::steady_clock::now();
        int  fps_count      = 0;
        double display_fps  = 0.0;

        while (running.load()) {
            const cv::Mat frame = camera.captureFrame();

            // Reiknar frame per second
            ++fps_count;
            const auto now = std::chrono::steady_clock::now();
            const double fps_elapsed = std::chrono::duration<double>(now - fps_window).count();
            if (fps_elapsed >= 1.0) {
                display_fps = fps_count / fps_elapsed;
                fps_count   = 0;
                fps_window  = now;
            }

            // Greining og rakning skotmarks
            const auto box = tracker.detectTarget(frame);

            int pan_rate  = 0;
            int tilt_rate = 0;

            if (box.has_value()) {
                modes.targetSeen();
                const cv::Point centre = tracker.centroid(*box);
                std::tie(pan_rate, tilt_rate) =
                    steering.computePanTilt(centre.x, centre.y, FRAME_CX, FRAME_CY);

                if (cfg.show_window) {
                    cv::rectangle(frame, *box, cv::Scalar(0, 255, 0), 2);
                    cv::circle(frame, centre, 4, cv::Scalar(0, 255, 0), -1);
                }
            } else {
                std::tie(pan_rate, tilt_rate) = steering.decay();

                const auto search_move = modes.handleLostTarget();
                if (search_move.has_value()) {
                    log_pico("SEARCH: ",
                             uart.move(search_move->first, search_move->second),
                             cfg.verbose);
                    last_update = now;   // Sleppir RATE í þessari umferð
                }
            }

            // Sendir RATE skipun
            const double since_update = std::chrono::duration<double>(now - last_update).count();
            if (since_update >= UPDATE_PERIOD_S && modes.getMode() != Mode::SEARCH) {
                log_pico("PICO: ", uart.rate(pan_rate, tilt_rate), cfg.verbose);
                last_update = now;
            }

            // Sýna hjálparglugga 
            if (cfg.show_window) {
                cv::line(frame, {FRAME_CX, 0},         {FRAME_CX, FRAME_HEIGHT}, {255, 0, 0}, 1);
                cv::line(frame, {0, FRAME_CY},          {FRAME_WIDTH, FRAME_CY}, {255, 0, 0}, 1);

                if (cfg.show_fps) {
                    std::ostringstream oss;
                    oss << "FPS " << std::fixed << std::setprecision(1) << display_fps;
                    cv::putText(frame, oss.str(), {20, FRAME_HEIGHT - 15},
                                cv::FONT_HERSHEY_SIMPLEX, 0.55, {255, 255, 0}, 2);
                }

                if (modes.getMode() == Mode::SEARCH)
                    cv::putText(frame, "SEARCHING", {20, 30},
                                cv::FONT_HERSHEY_SIMPLEX, 0.9, {0, 165, 255}, 2);

                cv::imshow("Igreypt kerfi", frame);
                if ((cv::waitKey(1) & 0xFF) == 'q')
                    break;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        cleanup();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        cleanup();
        return 1;
    }
}
