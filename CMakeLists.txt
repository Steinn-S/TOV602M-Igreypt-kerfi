cmake_minimum_required(VERSION 3.16)

project(igreypt_kerfi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEFINED ENV{OpenCV_DIR} AND NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "$ENV{OpenCV_DIR}" CACHE PATH "Directory containing OpenCVConfig.cmake")
endif()

find_package(OpenCV REQUIRED COMPONENTS core dnn highgui imgproc objdetect tracking videoio)
find_package(Threads REQUIRED)

add_executable(igreypt_kerfi
    src/main.cpp
    src/uart_link.cpp
    src/myndavel.cpp
    src/skotmark_tracking.cpp
    src/skotturns_stjornun.cpp
    src/mode_manager.cpp
)

target_include_directories(igreypt_kerfi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(igreypt_kerfi PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
)
