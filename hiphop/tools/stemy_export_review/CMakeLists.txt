cmake_minimum_required(VERSION 3.20)

add_executable(stemy_export_review main.cpp)
target_link_libraries(stemy_export_review PRIVATE stemy::dsp)
target_include_directories(stemy_export_review PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/dr_wav
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stemy_export_review PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(stemy_export_review PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/stemy_export_review
)
