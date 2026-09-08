qt_add_executable(
    pppps_poc_tests
    tst_PocPipeline.cpp
)

target_link_libraries(
    pppps_poc_tests
    PRIVATE
        pppps_poc_core
        Qt6::Test
)
pppps_enable_strict_warnings(pppps_poc_tests)

add_test(NAME pppps_poc_tests COMMAND pppps_poc_tests)
set_tests_properties(pppps_poc_tests PROPERTIES TIMEOUT 30)

add_test(NAME pppps_ui_smoke COMMAND PPPPS --smoke-test)
set_tests_properties(
    pppps_ui_smoke
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
)

qt_add_executable(
    pppps_segmentation_types_tests
    tst_SegmentationTypes.cpp
)
target_link_libraries(
    pppps_segmentation_types_tests
    PRIVATE
        pppps_poc_core
        Qt6::Test
)
pppps_enable_strict_warnings(pppps_segmentation_types_tests)
add_test(NAME pppps_segmentation_types_tests COMMAND pppps_segmentation_types_tests)
set_tests_properties(pppps_segmentation_types_tests PROPERTIES TIMEOUT 10)

qt_add_executable(
    pppps_image_canvas_tests
    tst_ImageCanvas.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ImageCanvas.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ImageCanvas.h
)
target_include_directories(pppps_image_canvas_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/app)
target_link_libraries(
    pppps_image_canvas_tests
    PRIVATE
        pppps_poc_core
        Qt6::Test
        Qt6::Widgets
)
pppps_enable_strict_warnings(pppps_image_canvas_tests)
add_test(NAME pppps_image_canvas_tests COMMAND pppps_image_canvas_tests)
set_tests_properties(
    pppps_image_canvas_tests
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
)

set(PPPPS_REAL_MODEL_TEST_IMAGE "${PROJECT_SOURCE_DIR}/poc/evaluation/data/OI-07.jpg")
if(EXISTS "${PPPPS_MASK_RCNN_MODEL}" AND EXISTS "${PPPPS_REAL_MODEL_TEST_IMAGE}")
    add_test(
        NAME pppps_real_mask_rcnn
        COMMAND
            pppps_segmentation_probe
            --model "${PPPPS_MASK_RCNN_MODEL}"
            --image "${PPPPS_REAL_MODEL_TEST_IMAGE}"
            --expect-label cat
            --probe-x 0.5
            --probe-y 0.5
            --output-dir "${CMAKE_BINARY_DIR}/poc-results/OI-07"
    )
    set_tests_properties(pppps_real_mask_rcnn PROPERTIES LABELS "real-model" TIMEOUT 180)
else()
    message(STATUS "Real Mask R-CNN test skipped until make models and make poc-data have run")
endif()
