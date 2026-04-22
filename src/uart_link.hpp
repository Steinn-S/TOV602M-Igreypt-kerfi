#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

// Sér um samskipti við Raspberry Pi Pico
class UartLink {
public:
    explicit UartLink(std::string port,
                      int baud = 115200,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
    ~UartLink();

    UartLink(const UartLink&)            = delete;
    UartLink& operator=(const UartLink&) = delete;

    void connect();
    void close();
    bool is_open() const;

    // Sendir skipun og les svar 
    std::vector<std::string>          sendCommand(const std::string& cmd,
                                                  std::chrono::milliseconds window =
                                                      std::chrono::milliseconds(500));
    std::map<std::string, std::string> readTelemetry();

    // Hjálparföll
    std::vector<std::string> enable(bool on);
    std::vector<std::string> stop();
    std::vector<std::string> rate(int pan, int tilt);
    std::vector<std::string> move(int pan, int tilt);

private:
    void configure_port();
    void write_all(const std::string& text);
    std::vector<std::string> drain(std::chrono::milliseconds window);

    std::string               port_;
    int                       baud_;
    std::chrono::milliseconds timeout_;
    int                       fd_;
    std::string               buf_;    // Afgangur frá fyrri lestri
};
