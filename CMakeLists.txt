cmake_minimum_required(VERSION 3.16)

project(AZPlayer VERSION 0.1 LANGUAGES CXX)

# 简单校验依赖，参数为：名称, 存放路径的变量
function(check_sdk_layout NAME PATH_VAR)
    set(ROOT_PATH "${${PATH_VAR}}")
    
    # 检查根路径是否存在
    if(NOT ROOT_PATH OR NOT EXISTS "${ROOT_PATH}")
        message(FATAL_ERROR 
            "\n[AZPlayer 错误]: 未能找到 ${NAME} 根目录！\n请参考README并检查 ./config/config.json 中的设置。\n")
    endif()

    # 检查必要的子目录
    set(SUB_DIRS "include" "lib" "bin")
    foreach(DIR ${SUB_DIRS})
        if(NOT EXISTS "${ROOT_PATH}/${DIR}")
            message(FATAL_ERROR 
                "\n[AZPlayer 错误]: ${NAME} 目录结构完整性校验失败！\n"
                "在路径: ${ROOT_PATH} 下找不到关键子目录: '${DIR}'。")
        endif()
    endforeach()

endfunction()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

# 从 config/env.json 读取配置
set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/config/env.json")
if(NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR "\n[AZPlayer 错误]: 未找到 ./config/env.json !!!请参考README\n")
endif()

# 使用 CMake 的 file 命令读取 JSON 文件并解析
file(READ "${CONFIG_FILE}" CONFIG_JSON_CONTENT)

# 提取 FFMPEG_DIR
string(JSON FFMPEG_DIR GET "${CONFIG_JSON_CONTENT}" "FFMPEG_DIR")
if(NOT FFMPEG_DIR OR NOT EXISTS "${FFMPEG_DIR}")
    message(FATAL_ERROR 
        "\n[AZPlayer 错误]: 未能从 env.json 中获取有效的 FFMPEG_DIR！\n"
        "当前值: ${FFMPEG_DIR}\n请参考README并检查 ./config/env.json 中的设置。\n")
endif()

# 提取 ASS_DIR
string(JSON ASS_DIR GET "${CONFIG_JSON_CONTENT}" "ASS_DIR")
if(NOT ASS_DIR OR NOT EXISTS "${ASS_DIR}")
    message(FATAL_ERROR 
        "\n[AZPlayer 错误]: 未能从 env.json 中获取有效的 ASS_DIR！\n"
        "当前值: ${ASS_DIR}\n请参考README并检查 ./config/env.json 中的设置。\n")
endif()

check_sdk_layout("FFmpeg" FFMPEG_DIR)
check_sdk_layout("libass" ASS_DIR)

# 设置 FFmpeg
set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)
# 设置 libass
set(ASS_INCLUDE_DIR ${ASS_DIR}/include)
set(ASS_LIB_DIR ${ASS_DIR}/lib)
set(ASS_BIN_DIR ${ASS_DIR}/bin)
set(app_icon_resource_windows "${CMAKE_CURRENT_SOURCE_DIR}/resource/icon/appico.rc")

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
)
qt_standard_project_setup()

if (MSVC)
    add_compile_options(/W4)
else() # GCC/Clang 下
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

qt_policy(SET QTP0001 NEW)

qt_add_executable(appAZPlayer
    main.cpp
    ${app_icon_resource_windows}
)


set_source_files_properties(
    qml/controls/AZTooltip.qml
    qml/playerState/AZPlayerState.qml
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
              qml/controls/AZWindow.qml
              qml/controls/AZEventBlocker.qml
              qml/fileDialog/AZFileDialog.qml
              qml/bottomBar/AZPlayerCtrlBar.qml
              qml/bottomBar/AZProgressBar.qml
              qml/bottomBar/AZBottomBar.qml
              qml/bottomBar/AZPlayerSetting.qml
              qml/topBar/AZPlayerAbout.qml
              qml/topBar/AZTopBar.qml
              qml/topBar/AZWindowControls.qml
              qml/sideBar/AZStreamView.qml
              qml/sideBar/AZSideBar.qml
              qml/sideBar/AZFileTab.qml
              qml/sideBar/AZSubtitleTab.qml
              qml/sideBar/AZAudioTab.qml
              qml/videoArea/AZVideoArea.qml
              qml/playerState/AZPlayerState.qml
              qml/videoArea/AZPlaybackStatsArea.qml
              qml/videoArea/AZVideoAngleDialArea.qml
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
            renderer/assrender.h renderer/assrender.cpp
            compat/compat.h
            types/types.h
            types/ptrs.h
            utils/utils.h
            utils/dirtyrectmanager.h utils/dirtyrectmanager.cpp
            utils/AtomicDoubleBuffer.h
            utils/filehelper.h utils/filehelper.cpp
            stats/playbackstats.h stats/playbackstats.cpp
            utils/spscbuffer.h utils/spscbuffer.cpp
            3rd/miniaudio/miniaudio.h 3rd/miniaudio/miniaudio.cpp
            utils/powermanager.h utils/powermanager.cpp
    RESOURCES resource.qrc    
)

# 宏标志
target_compile_definitions(appAZPlayer PRIVATE
    MA_USE_STDINT # 让 miniaudio 使用标准整数类型
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
        winmm
)

file(GLOB FFMPEG_DLLS
    "${FFMPEG_BIN_DIR}/avcodec-*.dll"
    "${FFMPEG_BIN_DIR}/avformat-*.dll"
    "${FFMPEG_BIN_DIR}/avutil-*.dll"
    "${FFMPEG_BIN_DIR}/swscale-*.dll"
    "${FFMPEG_BIN_DIR}/swresample-*.dll"
)

file(GLOB ASS_DLLS
    "${ASS_BIN_DIR}/*.dll"
)

add_custom_command(TARGET appAZPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        ${ASS_DLLS}
        $<TARGET_FILE_DIR:appAZPlayer>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSES
        $<TARGET_FILE_DIR:appAZPlayer>/LICENSES
    COMMAND_EXPAND_LISTS
)

include(GNUInstallDirs)
install(TARGETS appAZPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
