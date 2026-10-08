cmake_minimum_required(VERSION 3.16)
project(handheld_host_tests C)

enable_testing()
add_executable(test_fusion_policy
    test_fusion_policy.c
    ../../APP/DATA_FUSION/fusion_policy.c)
target_include_directories(test_fusion_policy PRIVATE ../../APP/DATA_FUSION)
add_test(NAME fusion_policy COMMAND test_fusion_policy)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(test_flash_store test_flash_store.c ../../APP/FLASH_STORE/flash_store.c)
    target_compile_definitions(test_flash_store PRIVATE STM32F10X_MD USE_STDPERIPH_DRIVER)
    target_include_directories(test_flash_store PRIVATE
        ../../APP/FLASH_STORE ../../Core ../../Libraries/CMSIS
        ../../Libraries/STM32F10x_StdPeriph_Driver/inc)
    add_test(NAME flash_store COMMAND test_flash_store)
endif()
