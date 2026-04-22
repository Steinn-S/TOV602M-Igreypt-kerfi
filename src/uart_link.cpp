#include "uart_link.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {

speed_t baud_constant(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
        default:
            throw std::runtime_error("Unsupported baud rate: " + std::to_string(baud));
    }
}

std::string trim_line(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

}  // nafnsvæði

UartLink::UartLink(std::string port, int baud, std::chrono::milliseconds timeout)
    : port_(std::move(port)), baud_(baud), timeout_(timeout), fd_(-1) {}

UartLink::~UartLink() { close(); }

void UartLink::connect() {
    if (is_open()) return;

    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0)
        throw std::runtime_error("Cannot open UART port " + port_ + ": " + std::strerror(errno));

    try {
        configure_port();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Hreinsar samskiptin áður en fyrsta skipun er send.
        write_all("\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        (void)drain(std::chrono::milliseconds(300));
    } catch (...) {
        close();
        throw;
    }
}

void UartLink::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    buf_.clear();
}

bool UartLink::is_open() const { return fd_ >= 0; }


std::vector<std::string> UartLink::sendCommand(const std::string& cmd,
                                                std::chrono::milliseconds window) {
    if (!is_open())
        throw std::runtime_error("UartLink: port not open");

    write_all(cmd + "\r\n");
    return drain(window);
}

std::map<std::string, std::string> UartLink::readTelemetry() {
    // Les STATUS og skilar gildum í töflu
    const auto lines = sendCommand("STATUS");
    std::map<std::string, std::string> result;
    for (const auto& line : lines) {
        std::istringstream ss(line);
        std::string token;
        while (ss >> token) {
            const auto eq = token.find('=');
            if (eq != std::string::npos)
                result[token.substr(0, eq)] = token.substr(eq + 1);
        }
    }
    return result;
}

// Hjálparföll

std::vector<std::string> UartLink::enable(bool on) {
    return sendCommand(on ? "ENABLE 1" : "ENABLE 0");
}

std::vector<std::string> UartLink::stop() {
    return sendCommand("STOP");
}

std::vector<std::string> UartLink::rate(int pan, int tilt) {
    return sendCommand("RATE " + std::to_string(pan) + " " + std::to_string(tilt));
}

std::vector<std::string> UartLink::move(int pan, int tilt) {
    return sendCommand("MOVE " + std::to_string(pan) + " " + std::to_string(tilt));
}

// Innri hjálparföll

void UartLink::configure_port() {
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0)
        throw std::runtime_error("tcgetattr failed: " + std::string(std::strerror(errno)));

    cfmakeraw(&tty);
    const speed_t speed = baud_constant(baud_);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = static_cast<cc_t>(std::clamp<long>(timeout_.count() / 100, 0, 255));

    if (tcsetattr(fd_, TCSANOW, &tty) != 0)
        throw std::runtime_error("tcsetattr failed: " + std::string(std::strerror(errno)));

    tcflush(fd_, TCIOFLUSH);
}

void UartLink::write_all(const std::string& text) {
    const char* ptr  = text.data();
    size_t      left = text.size();

    while (left > 0) {
        ssize_t n = ::write(fd_, ptr, left);
        if (n > 0) {
            ptr  += n;
            left -= static_cast<size_t>(n);
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } else {
            throw std::runtime_error("UART write failed: " + std::string(std::strerror(errno)));
        }
    }
    tcdrain(fd_);
}

std::vector<std::string> UartLink::drain(std::chrono::milliseconds window) {
    // Les þar til svar kemur eða tíminn klárast út
    std::vector<std::string> lines;
    const auto deadline = std::chrono::steady_clock::now() + window;

    auto extract = [&]() -> bool {
        for (;;) {
            auto nl = buf_.find('\n');
            if (nl == std::string::npos) return false;
            std::string line = trim_line(buf_.substr(0, nl));
            buf_.erase(0, nl + 1);
            if (!line.empty()) {
                lines.push_back(line);
                if (line.rfind("OK", 0) == 0 || line.rfind("ERR", 0) == 0)
                    return true;
            }
        }
    };

    if (extract()) return lines;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - std::chrono::steady_clock::now());
        timeval tv{static_cast<time_t>(us.count() / 1000000),
                   static_cast<suseconds_t>(us.count() % 1000000)};
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);

        const int ready = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) break;

        char buf[256];
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            buf_.append(buf, static_cast<size_t>(n));
            if (extract()) break;
        }
    }

    return lines;
}
