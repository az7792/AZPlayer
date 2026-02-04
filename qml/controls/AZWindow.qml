import QtQuick 2.15
import QtQuick.Controls 2.15

import Qt.labs.folderlistmodel
import QtQuick.Dialogs
Window {
    id: mainWin
    width: 800
    minimumWidth: 800
    height: 600
    minimumHeight: 480
    /**
     * 设置透明能避免在OpenGL下调整窗口大小导致的卡顿
     * 但是会产生新的问题:
     * 1. 全屏模式下使用AZTooltip会导致画面闪烁
     * 2. 打开文件管理器会不显示，需要在手动点击一次
     * > 全屏模式指 真全屏(Window.FullScreen) 或是 假全屏(Window.Windowed)但是屏幕矩形区域与显示器刚好一致
     * > 因此这儿全部采用假全屏且不隐藏 外围用于检测鼠标来resize window的区域(之前最大化下是会隐藏的)
    */
    color: "transparent"
    //color: "red"
    visibility: Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowMinimizeButtonHint

    // @warning anchors 时请加上 Margin 为 resizeBorderWidth
    // @warning 请确保 resizeBorderWidth >= 2
    // @warning 太大也不行 <20
    readonly property int resizeBorderWidth : 6 / Screen.devicePixelRatio

    // 需要背景颜色请修改这个
    property color backgroundColor: "green"


    // 全屏模式（主要是为了不被最小化所影响布局）
    property bool videoFull: false

    readonly property int winHidden: 0
    readonly property int winNormal: 2
    readonly property int winMinimized: 3
    readonly property int winMaximized: 4
    readonly property int winFullScreen: 5
    property int windowState: winNormal
    property int _lastWindowState: winNormal

    // normal 时的大小
    property rect _normalGeometry: Qt.rect(x, y, width, height)


    onActiveChanged: {
        if (active && windowState === winMinimized) {
            // 恢复最小化前状态
            windowState = _lastWindowState
            _lastWindowState = winMinimized
            console.log("激活");
        }
        if(!active && windowState !== winMinimized){
            // 最小化
            _lastWindowState = windowState
            windowState = winMinimized
            console.log("失活");
        }
    }

    // 最小化
    function minimize() {
        if (windowState === winMinimized) return

        _saveNormalGeometryIfNeeded()
        _lastWindowState = windowState
        windowState = winMinimized
        showMinimized() // 最小化使用Qt的方案
        console.log("最小化");
    }

    // 恢复
    function restore() {
        if (windowState === winNormal) return
        videoFull = false
        visibility = Window.Windowed

        _lastWindowState = windowState
        windowState = winNormal

        x = _normalGeometry.x
        y = _normalGeometry.y
        width  = _normalGeometry.width
        height = _normalGeometry.height
        console.log("恢复");
    }

    // 最大化
    function maximize() {
        if (windowState === winMaximized) return
        videoFull = false
        _saveNormalGeometryIfNeeded()

        visibility = Window.Windowed
        _lastWindowState = windowState
        windowState = winMaximized

        x = Screen.virtualX - resizeBorderWidth
        y = Screen.virtualY - resizeBorderWidth
        width  = Screen.desktopAvailableWidth + 2 * resizeBorderWidth
        height = Screen.desktopAvailableHeight + 2 * resizeBorderWidth
        console.log("最大化");
    }

    // 全屏
    function fullscreen() {
        if (windowState === winFullScreen) return
        videoFull = true
        _saveNormalGeometryIfNeeded()

        visibility = Window.Windowed

        _lastWindowState = windowState
        windowState = winFullScreen

        x = Screen.virtualX - resizeBorderWidth
        y = Screen.virtualY - resizeBorderWidth
        width  = Screen.width + 2 * resizeBorderWidth
        height = Screen.height + 2 * resizeBorderWidth
        console.log("全屏")
    }

    // 窗口移动
    function startMoveWindow(){
        mainWin.startSystemMove()
    }

    // 切换最大最小化
    function toggleMaximize(){
        if(windowState === winNormal)
            maximize()
        else
            restore()
    }



    // 背景
    Rectangle {
        anchors.fill: parent
        color: mainWin.backgroundColor
    }

    // 用于调整窗口大小
    MouseArea {
        property int resizeEdges: 0

        anchors.fill: parent
        hoverEnabled: true

        cursorShape: {
            if (!containsMouse) return Qt.ArrowCursor

            // 角
            if ((resizeEdges & Qt.LeftEdge)  && (resizeEdges & Qt.TopEdge))    return Qt.SizeFDiagCursor
            if ((resizeEdges & Qt.RightEdge) && (resizeEdges & Qt.BottomEdge)) return Qt.SizeFDiagCursor
            if ((resizeEdges & Qt.RightEdge) && (resizeEdges & Qt.TopEdge))    return Qt.SizeBDiagCursor
            if ((resizeEdges & Qt.LeftEdge)  && (resizeEdges & Qt.BottomEdge)) return Qt.SizeBDiagCursor

            // 边
            if (resizeEdges & (Qt.LeftEdge | Qt.RightEdge)) return Qt.SizeHorCursor
            if (resizeEdges & (Qt.TopEdge | Qt.BottomEdge)) return Qt.SizeVerCursor

            return Qt.ArrowCursor
        }

        onPositionChanged: (mouse) => updateResizeEdges(mouse.x, mouse.y)

        onPressed: (mouse) => {
            updateResizeEdges(mouse.x, mouse.y)
            if (resizeEdges) mainWin.startSystemResize(resizeEdges)
        }

        onExited: { resizeEdges = 0 }

        function updateResizeEdges(x, y) {
            let bSize = mainWin.resizeBorderWidth
            if (mainWin.windowState !== mainWin.winNormal || bSize <= 0) {
                resizeEdges = 0
                return
            }

            resizeEdges = 0
            if (x < bSize)           resizeEdges |= Qt.LeftEdge
            if (y < bSize)           resizeEdges |= Qt.TopEdge
            if (x >= width  - bSize) resizeEdges |= Qt.RightEdge
            if (y >= height - bSize) resizeEdges |= Qt.BottomEdge
        }
    }

    // 保存常规模式下的位置大小
    function _saveNormalGeometryIfNeeded() {
        if (windowState === winNormal) _normalGeometry = Qt.rect(x, y, width, height)
    }
}
