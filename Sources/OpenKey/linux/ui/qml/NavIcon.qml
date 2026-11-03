import QtQuick

Canvas {
    id: icon
    property string symbol: "home"
    property color ink: "#606060"
    implicitWidth: 20
    implicitHeight: 20
    onSymbolChanged: requestPaint()
    onInkChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.scale(width / 24, height / 24)
        ctx.strokeStyle = ink
        ctx.lineWidth = 1.7
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const paths = {
            home: [[[3,11],[12,3],[21,11]], [[6,10],[6,21],[10,21],[10,15],[14,15],[14,21],[18,21],[18,10]]],
            spelling: [[[3,20],[9,4],[15,20]], [[5,15],[13,15]], [[18,10],[22,10],[22,20]], [[18,15],[22,15]]],
            hotkeys: [[[3,6],[21,6],[21,18],[3,18],[3,6]], [[7,10],[7,10]], [[12,10],[12,10]], [[17,10],[17,10]], [[7,14],[17,14]]],
            compatibility: [[[3,3],[10,3],[10,10],[3,10],[3,3]], [[14,3],[21,3],[21,10],[14,10],[14,3]], [[3,14],[10,14],[10,21],[3,21],[3,14]], [[14,14],[21,14],[21,21],[14,21],[14,14]]],
            advanced: [[[4,6],[20,6]], [[4,12],[20,12]], [[4,18],[20,18]], [[8,3],[8,9]], [[16,9],[16,15]], [[10,15],[10,21]]],
            macro: [[[5,16],[16,5],[20,9],[9,20],[4,21],[5,16]], [[14,7],[18,11]]],
            convert: [[[4,7],[20,7],[16,3]], [[20,17],[4,17],[8,21]]],
            about: [[[12,10],[12,17]], [[12,6],[12,6]]],
            menu: [[[4,6],[20,6]], [[4,12],[20,12]], [[4,18],[20,18]]]
        }
        ctx.beginPath()
        for (const path of (paths[symbol] || paths.home)) {
            ctx.moveTo(path[0][0], path[0][1])
            for (let i = 1; i < path.length; ++i) ctx.lineTo(path[i][0], path[i][1])
        }
        if (symbol === "about") {
            ctx.moveTo(22,12)
            ctx.arc(12,12,10,0,Math.PI*2)
        }
        ctx.stroke()
    }
}
