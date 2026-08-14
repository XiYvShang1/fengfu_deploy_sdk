cmake_minimum_required(VERSION 3.10)
project(fengfu_loco_sdk VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FF_BUILD_EXAMPLES "Build Fengfu SDK examples" ON)
option(FF_BUILD_TESTS "Build Fengfu SDK offline tests" ON)

add_library(fengfu_loco_sdk STATIC
    src/fengfu_loco_client.c
    src/fengfu_loco_protocol.c
)
target_include_directories(fengfu_loco_sdk
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(fengfu_loco_sdk PRIVATE /O2 /W4)
else()
    target_compile_options(fengfu_loco_sdk PRIVATE -O2 -Wall -Wextra)
endif()

if(WIN32)
    target_link_libraries(fengfu_loco_sdk PUBLIC ws2_32)
endif()

if(FF_BUILD_EXAMPLES)
    add_executable(ai_loco_tool examples/ai_loco_tool.c)
    target_link_libraries(ai_loco_tool PRIVATE fengfu_loco_sdk)

    add_executable(loco_move_demo examples/loco_move_demo.cpp)
    target_link_libraries(loco_move_demo PRIVATE fengfu_loco_sdk)

    set_target_properties(ai_loco_tool loco_move_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

if(FF_BUILD_TESTS)
    enable_testing()
    add_executable(test_fengfu_loco_protocol tests/test_fengfu_loco_protocol.c)
    target_include_directories(test_fengfu_loco_protocol PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_fengfu_loco_protocol PRIVATE fengfu_loco_sdk)
    add_test(NAME fengfu_loco_protocol COMMAND test_fengfu_loco_protocol)
endif()

install(TARGETS fengfu_loco_sdk
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

