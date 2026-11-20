cmake_minimum_required(VERSION 3.24)
project(CustomSkinStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  add_compile_options(/utf-8 /bigobj /permissive-)
  add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Quick3D Multimedia Concurrent Quick3DHelpers)
qt_standard_project_setup(REQUIRES 6.5)

# ---- third party (fetched at configure time, same on macOS and Windows) ----
include(FetchContent)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
  URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
  SOURCE_SUBDIR build/cmake
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(xxhash
  URL https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR do_not_build)
FetchContent_Declare(miniz
  URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR do_not_build)
FetchContent_MakeAvailable(zstd xxhash miniz)

add_library(miniz_lib STATIC ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(miniz_lib PUBLIC ${miniz_SOURCE_DIR})

# ---- core: League file formats and edit logic (QtCore + QtGui only, no UI) ----
add_library(skincore STATIC
  src/core/Hashes.cpp
  src/core/Wad.cpp
  src/core/Fantome.cpp
  src/core/Bin.cpp
  src/core/Mesh.cpp
  src/core/Anm.cpp
  src/core/Tex.cpp
  src/core/Bc.cpp
  src/core/Wwise.cpp
  src/core/AudioDsp.cpp
  src/core/Recolor.cpp
  src/core/Vfx.cpp
  src/core/SkinProject.cpp
)
target_include_directories(skincore PUBLIC src ${xxhash_SOURCE_DIR})
target_link_libraries(skincore PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent libzstd_static miniz_lib)

option(SKINSTUDIO_APP "Build the GUI" ON)
if(SKINSTUDIO_APP)
# ---- the application ----
set(APP_PLATFORM_SOURCES)
if(WIN32)
  list(APPEND APP_PLATFORM_SOURCES platform/app.rc)
elseif(APPLE)
  set(APP_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/platform/icon/app.icns)
  set_source_files_properties(${APP_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  list(APPEND APP_PLATFORM_SOURCES ${APP_ICNS})
endif()
qt_add_executable(SkinStudio MACOSX_BUNDLE WIN32
  ${APP_PLATFORM_SOURCES}
  src/app/main.cpp
  src/app/AppController.cpp
  src/app/SkinGeometry.cpp
  src/app/WaveformItem.cpp
  src/app/AudioEngine.cpp
  src/app/ImageProvider.cpp
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(SkinStudio
  URI SkinStudio
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/StartScreen.qml
    qml/Editor.qml
    qml/Viewport.qml
    qml/Timeline.qml
    qml/ColorsPanel.qml
    qml/EffectsPanel.qml
    qml/VoicePanel.qml
    qml/LoadscreenPanel.qml
    qml/PackagePanel.qml
    qml/CropDialog.qml
    qml/controls/SButton.qml
    qml/controls/SSlider.qml
    qml/controls/SIcon.qml
    qml/controls/SSwatch.qml
    qml/controls/SToggle.qml
    qml/controls/SField.qml
  RESOURCES
    platform/icon/icon.png
  SOURCES
    src/app/AppController.h
    src/app/SkinGeometry.h
    src/app/WaveformItem.h
)
target_include_directories(SkinStudio PRIVATE src src/app)
target_link_libraries(SkinStudio PRIVATE skincore Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Multimedia)

set_target_properties(SkinStudio PROPERTIES
  OUTPUT_NAME "CustomSkinStudio"
  MACOSX_BUNDLE_GUI_IDENTIFIER io.github.ihasim.customskinstudio
  MACOSX_BUNDLE_BUNDLE_NAME "Custom Skin Studio"
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/Info.plist.in
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
  MACOSX_BUNDLE_ICON_FILE app.icns)

endif()

# ---- tests (format round-trips against a real .fantome) ----
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/core_tests.cpp)
  add_executable(skincore_tests tests/core_tests.cpp)
  target_link_libraries(skincore_tests PRIVATE skincore)
endif()

if(SKINSTUDIO_APP)
  install(TARGETS SkinStudio BUNDLE DESTINATION . RUNTIME DESTINATION bin)
endif()
