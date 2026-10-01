cmake_minimum_required(VERSION 3.16)
project(handheld_host_tests C)

enable_testing()
add_executable(test_fusion_policy
    test_fusion_policy.c
    ../../APP/DATA_FUSION/fusion_policy.c)
target_include_directories(test_fusion_policy PRIVATE ../../APP/DATA_FUSION)
add_test(NAME fusion_policy COMMAND test_fusion_policy)
