cmake_minimum_required(VERSION 3.20)
project(stemy_dsp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STEMY_BUILD_TESTS "Build unit tests" ON)
option(STEMY_BUILD_TOOLS "Build CLI tools" ON)

add_library(stemy_pffft STATIC third_party/pffft/pffft.c)
target_include_directories(stemy_pffft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pffft)
# pffft is C; silence unused-parameter noise on some platforms
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stemy_pffft PRIVATE -Wno-unused-parameter)
endif()

set(STEMY_DSP_SOURCES
  src/audio/audio_buffer.cpp
  src/audio/wav_io.cpp
  src/fft/pffft_fft.cpp
  src/analysis/loudness_analyzer.cpp
  src/analysis/peak_analyzer.cpp
  src/analysis/true_peak_analyzer.cpp
  src/analysis/true_peak_measure.cpp
  src/analysis/dynamics_analyzer.cpp
  src/analysis/spectral_analyzer.cpp
  src/analysis/stereo_analyzer.cpp
  src/analysis/analysis_pipeline.cpp
  src/dsp/biquad.cpp
  src/dsp/eq.cpp
  src/dsp/low_end_control.cpp
  src/dsp/compressor.cpp
  src/dsp/saturation.cpp
  src/dsp/soft_peak_rounder.cpp
  src/dsp/stereo_processor.cpp
  src/dsp/limiter.cpp
  src/dsp/true_peak_safety.cpp
  src/dsp/transient_controller.cpp
  src/dsp/gain_stage.cpp
  src/dsp/audio_safety.cpp
  src/adaptive/adaptive_engine.cpp
  src/genres/hiphop/hiphop_chain.cpp
  src/genres/hiphop/hiphop_parameters.cpp
  src/config/config_loader.cpp
  src/mastering/mastering_engine.cpp
  src/mastering/clean_loudness_refinement.cpp
)

add_library(stemy_dsp STATIC ${STEMY_DSP_SOURCES})
add_library(stemy::dsp ALIAS stemy_dsp)

target_include_directories(stemy_dsp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_wav
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann
)

target_link_libraries(stemy_dsp PRIVATE stemy_pffft)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stemy_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(STEMY_BUILD_TOOLS)
  add_subdirectory(tools/stemy_master)
  add_subdirectory(tools/stemy_stage_audit)
  add_subdirectory(tools/stemy_analyze)
  add_subdirectory(tools/stemy_export_review)
endif()

if(STEMY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
