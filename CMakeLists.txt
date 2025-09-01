cmake_minimum_required(VERSION 3.16)

project(AZPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 设置 FFmpeg 路径
set(FFMPEG_DIR D:/ffmpeg-8.0-full_build-shared)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)

qt_standard_project_setup()

qt_add_executable(appAZPlayer
    main.cpp
)

qt_add_qml_module(appAZPlayer
    URI AZPlayer
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES
        SOURCES
        SOURCES audioplayer.h audioplayer.cpp
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
)

# 添加库文件搜索路径
target_link_directories(appAZPlayer
    PRIVATE
        ${FFMPEG_LIB_DIR}
)

target_link_libraries(appAZPlayer
    PRIVATE
        Qt6::Quick
        Qt6::Gui
        Qt6::Core
        Qt6::Multimedia
        avcodec
        avformat
        avutil
        avdevice
        avfilter
        swscale
        swresample
)

add_custom_command(TARGET appAZPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFMPEG_BIN_DIR}/avutil-60.dll
            ${FFMPEG_BIN_DIR}/avcodec-62.dll
            ${FFMPEG_BIN_DIR}/avformat-62.dll
            ${FFMPEG_BIN_DIR}/avdevice-62.dll
            ${FFMPEG_BIN_DIR}/avfilter-11.dll
            ${FFMPEG_BIN_DIR}/swscale-9.dll
            ${FFMPEG_BIN_DIR}/swresample-6.dll
            $<TARGET_FILE_DIR:appAZPlayer>
        COMMENT "Copying FFmpeg DLLs to output directory"
    )

include(GNUInstallDirs)
install(TARGETS appAZPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
