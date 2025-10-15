import QtQuick

Item{

    property string streamType: "AUDIO"

    ListModel{
        id:listModel
    }

    function update(){
        var arr = []
        if(streamType === "AUDIO")
            arr = MediaCtrl.getAudioInfo()
        else if(streamType === "SUBTITLE")
            arr = MediaCtrl.getSubtitleInfo()

        listModel.clear()
        for(let i = 0;i<arr.length;++i){
            listModel.append(arr[i])
        }
        if(arr.length !== -1)
            view.setHighlightIdx(0)
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
