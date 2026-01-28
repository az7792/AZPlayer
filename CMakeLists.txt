cmake_minimum_required(VERSION 3.16)

project(AZPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)
# 设置 FFmpeg 路径
set(FFMPEG_DIR D:/ffmpeg-8.0-full_build-shared)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)
# 设置 libass 路径
set(ASS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libass)
set(ASS_INCLUDE_DIR ${ASS_DIR}/include)
set(ASS_LIB_DIR ${ASS_DIR}/lib)
set(ASS_BIN_DIR ${ASS_DIR}/bin)
set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/resource/icon/appico.rc")

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS QuickControls2)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)

if (MSVC)
    add_compile_options(/W4)
else() # GCC/Clang 下
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

qt_standard_project_setup()

qt_add_executable(appAZPlayer
    main.cpp
    ${app_icon_resource_windows}
)

#qt_policy(SET QTP0001 NEW)

set_source_files_properties(qml/controls/AZTooltip.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(appAZPlayer
    URI AZPlayer
    VERSION 1.0
    QML_FILES qml/Main.qml
              qml/controls/AZButton.qml
              qml/controls/AZDial.qml
              qml/controls/AZListView.qml
              qml/controls/AZSlider.qml
              qml/controls/AZTabBar.qml
              qml/controls/AZTextButton.qml
              qml/controls/AZTextInput.qml
              qml/controls/AZTooltip.qml
              qml/controls/AZCheckBox.qml
              qml/controls/AZComboBox.qml
              qml/fileDialog/AZFileDialog.qml
              qml/bottomBar/AZPlayerCtrlBar.qml
              qml/bottomBar/AZProgressBar.qml
              qml/bottomBar/AZBottomBar.qml
              qml/bottomBar/AZPlayerSetting.qml
              qml/topBar/AZPlayerAbout.qml
              qml/topBar/AZTopBar.qml
              qml/topBar/AZWindowControls.qml
              qml/sideBar/AZStreamView.qml
              qml/sideBar/AZFileSideBar.qml
              qml/sideBar/AZFileTab.qml
              qml/sideBar/AZSubtitleTab.qml
              qml/sideBar/AZAudioTab.qml
    SOURCES demux/demux.h demux/demux.cpp
            decode/decodebase.h decode/decodebase.cpp
            decode/decodeaudio.h decode/decodeaudio.cpp
            renderer/audioplayer.h renderer/audioplayer.cpp
            renderer/videorenderer.h renderer/videorenderer.cpp
            clock/globalclock.h clock/globalclock.cpp
            renderer/videoplayer.h renderer/videoplayer.cpp
            decode/decodevideo.h decode/decodevideo.cpp
            renderer/renderdata.h renderer/renderdata.cpp
            controller/mediacontroller.cpp controller/mediacontroller.h
            decode/decodesubtitle.h decode/decodesubtitle.cpp
    RESOURCES resource.qrc
    SOURCES renderer/assrender.h renderer/assrender.cpp
    SOURCES compat/compat.h
    SOURCES types/types.h
    SOURCES types/ptrs.h
    SOURCES utils/utils.h
    SOURCES utils/dirtyrectmanager.h utils/dirtyrectmanager.cpp
    SOURCES utils/AtomicDoubleBuffer.h
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appAZPlayer PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appAZPlayer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 添加头文件搜索路径
target_include_directories(appAZPlayer
    PRIVATE
        ${FFMPEG_INCLUDE_DIR}
        ${ASS_INCLUDE_DIR}
)

# 添加库文件搜索路径
target_link_directories(appAZPlayer
    PRIVATE
        ${FFMPEG_LIB_DIR}
        ${ASS_LIB_DIR}
)

target_link_libraries(appAZPlayer
    PRIVATE
        Qt6::QuickControls2
        Qt6::Quick
        Qt6::Gui
        Qt6::Core
        Qt6::Multimedia
        avcodec
        avformat
        avutil
        swscale
        swresample
        ass
        brotlicommon
        brotlidec
        brotlienc
        bz2
        freetype
        fribidi
        harfbuzz-subset
        harfbuzz
        libpng16
        zlib
)

add_custom_command(TARGET appAZPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFMPEG_BIN_DIR}/avutil-60.dll
            ${FFMPEG_BIN_DIR}/avcodec-62.dll
            ${FFMPEG_BIN_DIR}/avformat-62.dll
            ${FFMPEG_BIN_DIR}/swscale-9.dll
            ${FFMPEG_BIN_DIR}/swresample-6.dll
            ${ASS_BIN_DIR}/ass.dll
            ${ASS_BIN_DIR}/brotlicommon.dll
            ${ASS_BIN_DIR}/brotlidec.dll
            ${ASS_BIN_DIR}/brotlienc.dll
            ${ASS_BIN_DIR}/bz2.dll
            ${ASS_BIN_DIR}/freetype.dll
            ${ASS_BIN_DIR}/fribidi-0.dll
            ${ASS_BIN_DIR}/harfbuzz-subset.dll
            ${ASS_BIN_DIR}/harfbuzz.dll
            ${ASS_BIN_DIR}/libpng16.dll
            ${ASS_BIN_DIR}/zlib1.dll
            $<TARGET_FILE_DIR:appAZPlayer>        
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSES
            $<TARGET_FILE_DIR:appAZPlayer>/LICENSES
        COMMENT "拷贝DLL和LICENSES"
    )

include(GNUInstallDirs)
install(TARGETS appAZPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
