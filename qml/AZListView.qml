import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

Rectangle {
    id:root
    clip: true

    // 记录当前播放和单击选择
    property int activeIndex: -1
    property ListModel listModel: null

    signal delActive()
    signal stopActive()
    signal openIndex(int index)

    function setHighlightIdx(idx){
        activeIndex = idx;
        listView.currentIndex = idx
    }

    function stopActiveItem(){
        if(activeIndex === -1) return

        activeIndex = -1
        stopActive()

    }

    function openNext(){
        if(activeIndex === -1) return

        activeIndex = (activeIndex + 1) % listView.count
        openIndex(activeIndex)
        listView.currentIndex = activeIndex
    }

    function openPrev(){
        if(activeIndex === -1) return

        activeIndex = (activeIndex - 1 + listView.count) % listView.count
        openIndex(activeIndex)
        listView.currentIndex = activeIndex
    }

    ListView {
        id: listView
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        highlight:Rectangle{
            width: listView.width
            height: 30
            color: listView.focus ? "#2b2b2b" : "#444444"
        }
        model: listModel

        ScrollBar.vertical: ScrollBar {
            id:scrollBar
            policy: ScrollBar.AsNeeded
            background: Rectangle {
                color: "#222222"
            }
            contentItem: Rectangle {
                implicitWidth: 8
                color: scrollBar.pressed ? "#666666" : "#888888"
            }
        }

        delegate: Text {
            id: textItem
            width: listView.width
            height: 20
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: " " + (index+1) + ". " + model.text
            color: index == root.activeIndex ? "#2b82d5" : (index == listView.currentIndex ? "#f0f0f0" : "#747474")

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = index
                    listView.forceActiveFocus()
                }
                onDoubleClicked: {
                    if(root.activeIndex !== index){
                        root.activeIndex = index
                        openIndex(root.activeIndex)
                    }
                }
            }
        }

        Keys.onPressed:function(event) {
            if (event.key === Qt.Key_Delete && listView.currentIndex >= 0) {
                //更新播放索引
                if(listView.currentIndex === root.activeIndex){
                    delActive()
                    root.activeIndex = -1
                    // close
                }else if(listView.currentIndex < root.activeIndex){
                    root.activeIndex--
                }

                listModel.remove(listView.currentIndex)

                // 更新选中索引
                if (listView.currentIndex >= listModel.count) {
                    listView.currentIndex = listModel.count - 1
                }

                // 防止事件继续传递
                event.accepted = true
            }
        }

    }

}
