// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import "../controls"
// 加载展示媒体流信息（音频或字幕）
Item{

    property string streamType: "AUDIO" // 或 "SUBTITLE"
    property int currentIdx: -1

    ListModel{
        id:listModel
    }

    function update(){
        var arr = []
        if(streamType === "AUDIO"){
            arr = MediaCtrl.getAudioInfo()
            currentIdx = MediaCtrl.getAudioIdx()
        } else if(streamType === "SUBTITLE"){
            arr = MediaCtrl.getSubtitleInfo()
            currentIdx = MediaCtrl.getSubtitleIdx()
        }

        listModel.clear()
        for(let i = 0;i<arr.length;++i){
            listModel.append(arr[i])
        }
        if(arr.length !== -1)
            view.setHighlightIdx(currentIdx)
    }

    AZListView{
        id:view
        color: "#3b3b3b"
        anchors.fill: parent
        listModel: listModel
        readOnly: true
        onOpenIndex:function(index){
            let val = listModel.get(index)
            if(streamType === "AUDIO")
                MediaCtrl.switchAudioStream(val.demuxIdx,val.streamIdx)
            else if(streamType === "SUBTITLE")
                MediaCtrl.switchSubtitleStream(val.demuxIdx,val.streamIdx)
        }
        Connections{
            target: MediaCtrl
            function onStreamInfoUpdate(){
                update()
            }
        }
    }
}
