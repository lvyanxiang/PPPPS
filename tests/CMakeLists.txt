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
