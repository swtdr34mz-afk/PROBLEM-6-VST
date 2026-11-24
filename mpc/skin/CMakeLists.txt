# The MPC skin's renderer: the VST3's own panel (src/PluginEditor.cpp, src/ui/), drawn at the MPC's screen size, and
# every control cut out at every value it can take (film strips, button images), with a manifest for make_skin.py.
#   cmake -S mpc/skin -B build-skin -DP67_JUCE_DIR=/path/to/JUCE && cmake --build build-skin
#   build-skin/p67_skin_render mpc/build/skin-render
#   python3 mpc/skin/make_skin.py mpc/build/skin-render mpc/plugin/skins
# (Only when the panel changes: the result, mpc/plugin/skins/, is kept in the repository.)
cmake_minimum_required(VERSION 3.22)
project(P67SkinRender VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(P67_JUCE_DIR "" CACHE PATH "a JUCE checkout (empty: from GitHub)")
if(P67_JUCE_DIR)
    add_subdirectory(${P67_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(R ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_console_app(p67_skin_render PRODUCT_NAME "p67_skin_render")
juce_add_binary_data(P67SkinFonts SOURCES
    ${R}/fonts/FiraSans-SemiBold.ttf
    ${R}/fonts/FiraSans-Bold.ttf
    ${R}/fonts/FiraSans-ExtraBold.ttf
    ${R}/fonts/AnonymousPro-Bold.ttf)
target_sources(p67_skin_render PRIVATE
    render_skin.cpp
    ${R}/src/P67Engine.cpp
    ${R}/src/PluginProcessor.cpp
    ${R}/src/PluginEditor.cpp
    ${R}/src/ui/Look.cpp
    ${R}/src/ui/Controls.cpp
    ${R}/dsp/params.c
    ${R}/dsp/presets.c)
target_include_directories(p67_skin_render PRIVATE ${R}/shim ${R}/dsp ${R}/src)
target_compile_definitions(p67_skin_render PRIVATE
    P67_PLUGIN=1 PLAT_HOST=1
    JucePlugin_Name="PROPHET 67" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0
    JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(p67_skin_render PRIVATE P67SkinFonts juce::juce_audio_utils juce::juce_recommended_config_flags)
