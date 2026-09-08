cmake_minimum_required(VERSION 3.25)

project(
    PPPPS
    VERSION 0.1.0
    DESCRIPTION "Object-driven intelligent image editor POC"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PPPPS_FETCH_OPENCV "Fetch a pinned minimal OpenCV when it is not installed" ON)
set(
    PPPPS_MASK_RCNN_MODEL
    "${PROJECT_SOURCE_DIR}/poc/models/MaskRCNN-12-int8.onnx"
    CACHE FILEPATH
    "Default Mask R-CNN ONNX model used by the M0 POC"
)

function(pppps_enable_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

include(CTest)
include(FetchContent)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()

find_package(OpenCV 4 QUIET COMPONENTS core imgproc)
if(OpenCV_FOUND)
    set(PPPPS_OPENCV_LIBRARIES ${OpenCV_LIBS})
    set(PPPPS_OPENCV_INCLUDE_DIRECTORIES ${OpenCV_INCLUDE_DIRS})
    message(STATUS "Using installed OpenCV ${OpenCV_VERSION}")
elseif(PPPPS_FETCH_OPENCV)
    message(STATUS "Installed OpenCV not found; fetching pinned minimal OpenCV 4.12.0")

    set(BUILD_LIST core,imgproc CACHE STRING "OpenCV modules required by PPPPS" FORCE)
    set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_js OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python2 OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python_bindings_generator OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python_tests OFF CACHE BOOL "" FORCE)
    set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_JAVA OFF CACHE BOOL "" FORCE)
    set(BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(WITH_ITT OFF CACHE BOOL "" FORCE)
    set(WITH_IPP OFF CACHE BOOL "" FORCE)
    set(WITH_AVFOUNDATION OFF CACHE BOOL "" FORCE)
    set(WITH_FFMPEG OFF CACHE BOOL "" FORCE)
    set(WITH_LAPACK OFF CACHE BOOL "" FORCE)
    set(WITH_OPENCL OFF CACHE BOOL "" FORCE)
    set(WITH_OPENEXR OFF CACHE BOOL "" FORCE)
    set(WITH_OPENJPEG OFF CACHE BOOL "" FORCE)
    set(WITH_PROTOBUF OFF CACHE BOOL "" FORCE)
    set(WITH_VTK OFF CACHE BOOL "" FORCE)
    set(WITH_JPEG OFF CACHE BOOL "" FORCE)
    set(WITH_PNG OFF CACHE BOOL "" FORCE)
    set(WITH_TIFF OFF CACHE BOOL "" FORCE)
    set(WITH_WEBP OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        opencv
        URL https://github.com/opencv/opencv/archive/49486f61fb25722cbcf586b7f4320921d46fb38e.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(opencv)
    set(PPPPS_OPENCV_LIBRARIES opencv_core opencv_imgproc)
    set(
        PPPPS_OPENCV_INCLUDE_DIRECTORIES
        ${opencv_SOURCE_DIR}/modules/core/include
        ${opencv_SOURCE_DIR}/modules/imgproc/include
        ${CMAKE_BINARY_DIR}
    )
else()
    message(FATAL_ERROR "OpenCV 4 core/imgproc was not found and PPPPS_FETCH_OPENCV is OFF")
endif()

find_package(onnxruntime CONFIG QUIET)
if(TARGET onnxruntime AND NOT TARGET onnxruntime::onnxruntime)
    add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
endif()

if(NOT TARGET onnxruntime::onnxruntime)
    find_path(
        ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATH_SUFFIXES include include/onnxruntime
    )
    find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime)

    if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
        message(FATAL_ERROR
            "ONNX Runtime was not found. Set CMAKE_PREFIX_PATH or ONNXRUNTIME_ROOT "
            "to an installation containing onnxruntime_cxx_api.h and libonnxruntime."
        )
    endif()

    add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED)
    set_target_properties(
        onnxruntime::onnxruntime
        PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()

qt_standard_project_setup()

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
