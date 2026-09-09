qt_add_library(
    pppps_poc_core STATIC
    core/PocPipeline.cpp
    core/PocTaskController.cpp
    core/MaskRcnnEngine.cpp
    core/ModnetEngine.cpp
    core/MattingTaskController.cpp
    core/MattingTypes.cpp
    core/AlphaMaskEditor.cpp
    core/SegmentationTaskController.cpp
    core/SegmentationTypes.cpp
    include/pppps/poc/PocPipeline.h
    include/pppps/poc/PocTaskController.h
    include/pppps/segmentation/MaskRcnnEngine.h
    include/pppps/segmentation/SegmentationTaskController.h
    include/pppps/segmentation/SegmentationTypes.h
    include/pppps/matting/ModnetEngine.h
    include/pppps/matting/MattingTaskController.h
    include/pppps/matting/MattingTypes.h
    include/pppps/matting/AlphaMaskEditor.h
)

target_include_directories(
    pppps_poc_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

target_include_directories(
    pppps_poc_core
    SYSTEM PRIVATE
        ${PPPPS_OPENCV_INCLUDE_DIRECTORIES}
)

target_link_libraries(
    pppps_poc_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        ${PPPPS_OPENCV_LIBRARIES}
        onnxruntime::onnxruntime
)

target_compile_features(pppps_poc_core PUBLIC cxx_std_20)
pppps_enable_strict_warnings(pppps_poc_core)

qt_add_executable(
    PPPPS
    app/main.cpp
    app/MainWindow.cpp
    app/MainWindow.h
    app/ImageCanvas.cpp
    app/ImageCanvas.h
)

target_link_libraries(
    PPPPS
    PRIVATE
        pppps_poc_core
        Qt6::Widgets
)
pppps_enable_strict_warnings(PPPPS)
target_compile_definitions(
    PPPPS
    PRIVATE
        PPPPS_DEFAULT_MODEL_PATH="${PPPPS_MASK_RCNN_MODEL}"
        PPPPS_DEFAULT_MODNET_MODEL_PATH="${PPPPS_MODNET_MODEL}"
)

qt_add_executable(
    pppps_segmentation_probe
    tools/SegmentationProbe.cpp
)
target_link_libraries(
    pppps_segmentation_probe
    PRIVATE
        pppps_poc_core
        Qt6::Gui
)
pppps_enable_strict_warnings(pppps_segmentation_probe)

qt_add_executable(
    pppps_matting_probe
    tools/MattingProbe.cpp
)
target_link_libraries(
    pppps_matting_probe
    PRIVATE
        pppps_poc_core
        Qt6::Gui
)
pppps_enable_strict_warnings(pppps_matting_probe)

set_target_properties(
    PPPPS
    PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PPPPS POC"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.pppps.poc"
        WIN32_EXECUTABLE TRUE
)

install(TARGETS PPPPS BUNDLE DESTINATION . RUNTIME DESTINATION bin)
