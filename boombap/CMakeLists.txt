cmake_minimum_required(VERSION 3.22)

project(MasteringService)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(PROJECT_VERSION  1.0.0)

include(FetchContent)

FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.3
        GIT_PROGRESS 1
        GIT_SHALLOW 1)

FetchContent_MakeAvailable(JUCE)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h" )

juce_add_console_app(${CMAKE_PROJECT_NAME}
        PRODUCT_NAME "${CMAKE_PROJECT_NAME}"
        NEEDS_WEB_BROWSER FALSE
        NEEDS_CURL FALSE)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SourceFiles})

target_include_directories(${CMAKE_PROJECT_NAME}
        SYSTEM PRIVATE
        source
)

target_compile_definitions(${CMAKE_PROJECT_NAME}
        PUBLIC
        APPLICATION_VERSION="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_JACK=0
        JUCE_USE_FONTCONFIG=0
        JUCE_USE_FREETYPE=0
        JUCE_USE_XCURSOR=0
        JUCE_USE_XRANDR=0
        JUCE_USE_XRENDER=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XINPUT=0
)

target_link_libraries(${CMAKE_PROJECT_NAME}
        PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_formats
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


