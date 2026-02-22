cmake_minimum_required(VERSION 3.20)

project(ASCII_Encoder_and_Decoder)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set vcpkg toolchain file if not already set
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    else()
        message(FATAL_ERROR "VCPKG_ROOT environment variable is not set. Please set it to your vcpkg installation directory.")
    endif()
endif()

# Enable vcpkg manifest mode (automatically installs dependencies from vcpkg.json)
set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_MANIFEST_DIR ${CMAKE_SOURCE_DIR})
message(STATUS "Vcpkg manifest mode enabled - dependencies will be installed automatically from vcpkg.json")

# Create build directory if running CMake directly
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})

# Find required packages
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

# Create main executable
add_executable(ascii_art src/main.cpp)
target_include_directories(ascii_art PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ascii_art PRIVATE 
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    opencv_core
    opencv_highgui
    opencv_imgproc
    opencv_videoio
)
set_target_properties(ascii_art PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ascii_art>"
)

# Create test executable
add_executable(test_codec src/test_codec.cpp)
target_include_directories(test_codec PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(test_codec PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_codec>"
)

# Create simple test executable
add_executable(simple_test src/simple_test.cpp)
target_include_directories(simple_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(simple_test PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:simple_test>"
)

# Copy assets to build directory
add_custom_command(TARGET ascii_art POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ascii_art>/assets
)

# Copy SDL3 DLLs to output directory on Windows
if(WIN32)
    add_custom_command(TARGET ascii_art POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:ascii_art>
    )
endif()
