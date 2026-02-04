pragma Singleton
import QtQuick 2.15
import "../fileDialog"

Item {
    property string currentFile: ""            // 当前播放的媒体文件
    property real videoX: 0                    // 视频x
    property real videoY: 0                    // 视频y
    property real videoAngle:0.0               // 视频顺时针旋转(角度)
    property int  videoScale:100               // 缩放(100为不缩放)
    property bool videoShowSubtitle: true      // 是否显示字幕
    property bool videoHorizontalMirror: false // 水平镜像
    property bool videoVerticalMirror: false   // 垂直镜像

    property alias mediaListModel: fileDialog.mediaListModel
    property alias mediafileDialog: fileDialog

    // FileDialog
    AZFileDialog{
        id: fileDialog
    }
}
