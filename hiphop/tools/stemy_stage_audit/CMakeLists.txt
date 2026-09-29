cmake_minimum_required(VERSION 3.20)

add_executable(stemy_stage_audit main.cpp)
target_link_libraries(stemy_stage_audit PRIVATE stemy::dsp)
target_include_directories(stemy_stage_audit PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/nlohmann
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stemy_stage_audit PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(stemy_stage_audit PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/stemy_stage_audit
)
