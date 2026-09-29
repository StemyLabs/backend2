cmake_minimum_required(VERSION 3.20)

# Prefer vendored GoogleTest (offline / handoff builds). Fall back to FetchContent.
set(STEMY_GTEST_VENDOR "${CMAKE_SOURCE_DIR}/third_party/googletest")
if(EXISTS "${STEMY_GTEST_VENDOR}/CMakeLists.txt")
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  add_subdirectory("${STEMY_GTEST_VENDOR}" "${CMAKE_BINARY_DIR}/_deps/googletest-build")
else()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(stemy_tests
  test_main.cpp
  audio/audio_buffer_test.cpp
  audio/validation_test.cpp
  analysis/analyzers_test.cpp
  analysis/stereo_analyzer_test.cpp
  analysis/true_peak_measure_test.cpp
  dsp/eq_test.cpp
  dsp/compressor_test.cpp
  dsp/saturation_test.cpp
  dsp/stereo_test.cpp
  dsp/limiter_test.cpp
  dsp/true_peak_safety_test.cpp
  dsp/channel_balance_test.cpp
  dsp/transient_test.cpp
  dsp/mono_compat_test.cpp
  adaptive/adaptive_test.cpp
  adaptive/m2_adaptive_test.cpp
  mastering/clean_loudness_test.cpp
  genres/hiphop_chain_test.cpp
  e2e/e2e_test.cpp
  regression/determinism_test.cpp
)

target_link_libraries(stemy_tests PRIVATE stemy::dsp GTest::gtest GTest::gtest_main)
target_include_directories(stemy_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/dr_wav
)
target_compile_definitions(stemy_tests PRIVATE
  STEMY_REPO_ROOT="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(stemy_tests)
