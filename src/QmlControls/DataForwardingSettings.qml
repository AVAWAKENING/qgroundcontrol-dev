/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.Comms

Rectangle {
    id:                 root
    implicitWidth:      ScreenTools.defaultFontPixelWidth * 55
    implicitHeight:     ScreenTools.defaultFontPixelHeight * 32
    width:              implicitWidth
    height:             implicitHeight
    color:              qgcPal.windowShade
    radius:             ScreenTools.defaultFontPixelWidth / 2
    border.width:       1
    border.color:       qgcPal.windowShadeLight

    property real _margins: ScreenTools.defaultFontPixelHeight * 0.5
    property var popupParent: null

    signal closeClicked()

    QGCPalette { id: qgcPal }

    Settings {
        id: settings
        property string frequency:       "20"
        property string ipAddress:      ""
        property string portNumber:     ""
        property string zdNumber:       ""
        property string radarId:        ""
        property string originLat:      ""
        property string originLon:      ""
        property string originAlt:      ""
        property bool   forwardingEnabled: false
    }

    Connections {
        target: DataForwardingSender

        function onForwardingStarted() {
            settings.forwardingEnabled = true
            statusText.text = qsTr("转发中：") + settings.ipAddress + ":" + settings.portNumber + " @" + settings.frequency + "Hz"
            console.log("DataForwarding: Forwarding started")
        }

        function onForwardingStopped() {
            settings.forwardingEnabled = false
            statusText.text = qsTr("已停止")
            console.log("DataForwarding: Forwarding stopped")
        }

        function onErrorOccurred(errorString) {
            statusText.text = qsTr("错误：") + errorString
            settings.forwardingEnabled = false
            console.warn("DataForwarding error:", errorString)
        }

        function onDataSent(data) {
            // 可选：记录数据发送事件（避免频繁日志）
        }
    }

    function validateInputs() {
        var lat = parseFloat(settings.originLat)
        if (settings.originLat !== "" && (isNaN(lat) || lat < -90 || lat > 90)) {
            statusText.text = qsTr("错误：纬度必须在-90到90之间")
            return false
        }

        var lon = parseFloat(settings.originLon)
        if (settings.originLon !== "" && (isNaN(lon) || lon < -180 || lon > 180)) {
            statusText.text = qsTr("错误：经度必须在-180到180之间")
            return false
        }

        var port = parseInt(settings.portNumber)
        if (isNaN(port) || port < 1 || port > 65535) {
            statusText.text = qsTr("错误：端口号必须在1-65535之间")
            return false
        }

        var freq = parseFloat(settings.frequency)
        if (isNaN(freq) || freq < 0.1 || freq > 100) {
            statusText.text = qsTr("错误：频率必须在0.1-100Hz之间")
            return false
        }

        var ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/
        if (!ipPattern.test(settings.ipAddress)) {
            statusText.text = qsTr("错误：IP地址格式不正确")
            return false
        }

        var ipParts = settings.ipAddress.split('.')
        for (var i = 0; i < ipParts.length; i++) {
            var part = parseInt(ipParts[i])
            if (isNaN(part) || part < 0 || part > 255) {
                statusText.text = qsTr("错误：IP地址每段必须在0-255之间")
                return false
            }
        }

        return true
    }

    MouseArea {
        id: resizeMouseArea
        anchors.right:    parent.right
        anchors.bottom:   parent.bottom
        width:            ScreenTools.defaultFontPixelWidth * 2
        height:           ScreenTools.defaultFontPixelHeight * 2
        cursorShape:      Qt.SizeFDiagCursor

        property real lastX: 0
        property real lastY: 0

        onPressed: {
            lastX = mouse.x
            lastY = mouse.y
        }

        onMouseXChanged: {
            var dx = mouse.x - lastX
            var newWidth = root.width + dx
            if (newWidth >= ScreenTools.defaultFontPixelWidth * 40) {
                root.width = newWidth
            }
        }

        onMouseYChanged: {
            var dy = mouse.y - lastY
            var newHeight = root.height + dy
            if (newHeight >= ScreenTools.defaultFontPixelHeight * 20) {
                root.height = newHeight
            }
        }
    }

    Item {
        id: dragArea
        anchors.top:    parent.top
        anchors.left:   parent.left
        anchors.right:  parent.right
        height:         ScreenTools.defaultFontPixelHeight * 2.5
        z:              100

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: ScreenTools.defaultFontPixelHeight * 0.3
            width: ScreenTools.defaultFontPixelWidth * 20
            height: ScreenTools.defaultFontPixelHeight * 0.4
            color: "white"
            radius: height / 2
            opacity: 0.7
        }
    }

    QGCFlickable {
        id:                 flickable
        anchors.topMargin:  _margins
        anchors.bottomMargin: _margins
        anchors.leftMargin:  _margins
        anchors.rightMargin: _margins
        anchors.top:         dragArea.bottom
        anchors.left:        parent.left
        anchors.right:       parent.right
        anchors.bottom:      parent.bottom
        contentWidth:       mainLayout.width
        contentHeight:      mainLayout.height
        flickableDirection: Flickable.VerticalFlick
        clip:               true

        ColumnLayout {
            id:                 mainLayout
            width:              flickable.width - _margins * 2
            spacing:            ScreenTools.defaultFontPixelHeight * 0.5

            RowLayout {
                Layout.fillWidth: true
                spacing:          ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               qsTr("转发设置")
                    font.pointSize:     ScreenTools.mediumFontPointSize
                    font.bold:          true
                }

                QGCLabel { text: qsTr("转发开关:") }

                QGCSwitch {
                    id:                 forwardingSwitch
                    checked:            settings.forwardingEnabled
                    onCheckedChanged: {
                        if (checked) {
                            if (!validateInputs()) {
                                forwardingSwitch.checked = false
                                return
                            }

                            settings.forwardingEnabled = true
                            var originLat = parseFloat(settings.originLat) || 0.0
                            var originLon = parseFloat(settings.originLon) || 0.0
                            var originAlt = parseFloat(settings.originAlt) || 0.0
                            var freq = parseFloat(settings.frequency) || 1.0
                            var radarId = parseInt(settings.radarId) || 0
                            var deviceId = parseInt(settings.zdNumber) || 0
                            DataForwardingSender.startForwarding(settings.ipAddress, parseInt(settings.portNumber), freq, originLat, originLon, originAlt, radarId, deviceId)
                        } else {
                            settings.forwardingEnabled = false
                            DataForwardingSender.stopForwarding()
                        }
                    }
                }

                QGCButton {
                    text:               qsTr("关闭")
                    onClicked:          root.closeClicked()
                }
            }

            GridLayout {
                Layout.fillWidth:   true
                columns:            2
                rowSpacing:         ScreenTools.defaultFontPixelHeight * 0.25
                columnSpacing:      ScreenTools.defaultFontPixelWidth

                QGCLabel { text: qsTr("发送频率 (Hz):") }
                QGCTextField {
                    id:                 frequencyField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：20")
                    text:               settings.frequency
                    onTextChanged:      settings.frequency = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("IP 地址:") }
                QGCTextField {
                    id:                 ipField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：192.168.1.100")
                    text:               settings.ipAddress
                    onTextChanged:      settings.ipAddress = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("端口号:") }
                QGCTextField {
                    id:                 portField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：8987")
                    text:               settings.portNumber
                    onTextChanged:      settings.portNumber = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("ZD 编号:") }
                QGCTextField {
                    id:                 zdField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("输入 ZD 编号")
                    text:               settings.zdNumber
                    onTextChanged:      settings.zdNumber = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("雷达标识:") }
                QGCTextField {
                    id:                 radarField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("输入雷达标识")
                    text:               settings.radarId
                    onTextChanged:      settings.radarId = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("原点纬度:") }
                QGCTextField {
                    id:                 latField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：41.0774773")
                    text:               settings.originLat
                    onTextChanged:      settings.originLat = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("原点经度:") }
                QGCTextField {
                    id:                 lonField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：100.5217878")
                    text:               settings.originLon
                    onTextChanged:      settings.originLon = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }

                QGCLabel { text: qsTr("原点大地高度:") }
                QGCTextField {
                    id:                 altField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("例如：1046.505")
                    text:               settings.originAlt
                    onTextChanged:      settings.originAlt = text
                    enabled:            !settings.forwardingEnabled
                    opacity:            settings.forwardingEnabled ? 0.7 : 1.0
                }
            }

            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: statusText.implicitHeight + ScreenTools.defaultFontPixelHeight * 2
                color:                  qgcPal.button
                border.width:           1
                border.color:           qgcPal.buttonBorder
                radius:                 ScreenTools.defaultFontPixelWidth / 4

                QGCLabel {
                    id:                 statusText
                    anchors.centerIn:   parent
                    text:               qsTr("转发状态显示栏")
                    color:              "white"
                    font.pointSize:     ScreenTools.defaultFontPointSize
                    font.bold:          true
                }
            }
        }
    }

    Rectangle {
        anchors.right:    parent.right
        anchors.bottom:   parent.bottom
        width:            ScreenTools.defaultFontPixelWidth * 1.5
        height:           width
        color:            "transparent"

        Canvas {
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = qgcPal.text
                ctx.lineWidth = 2

                ctx.beginPath()
                ctx.moveTo(width * 0.7, height * 0.3)
                ctx.lineTo(width * 0.3, height * 0.7)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(width * 0.9, height * 0.3)
                ctx.lineTo(width * 0.3, height * 0.9)
                ctx.stroke()
            }
        }
    }

    Component.onCompleted: {
        statusText.text = ""
    }
}
