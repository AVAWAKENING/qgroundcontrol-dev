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

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

Rectangle {
    id:                 root
    implicitWidth:      ScreenTools.defaultFontPixelWidth * 40
    implicitHeight:     ScreenTools.defaultFontPixelHeight * 25
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

    Connections {
        target: QGroundControl.measureDistanceController
        function onEnabledChanged(enabled) {
            measureDistanceButton.checked = enabled
            updateMeasureDistanceStatusText()
        }
        function onDistanceChanged() {
            updateMeasureDistanceStatusText()
        }
        function onStartPointChanged() {
            updateMeasureDistanceStatusText()
        }
        function onEndPointChanged() {
            updateMeasureDistanceStatusText()
        }
    }

    function updateMeasureDistanceStatusText() {
        if (!QGroundControl.measureDistanceController.enabled) {
            statusText.text = qsTr("测距工具已禁用")
            return
        }

        if (QGroundControl.measureDistanceController.hasValidStart && QGroundControl.measureDistanceController.hasValidEnd) {
            var dist = QGroundControl.measureDistanceController.distanceString()
            statusText.text = qsTr("距离: ") + dist
        } else if (QGroundControl.measureDistanceController.hasValidStart) {
            statusText.text = qsTr("起点已标记,请点击地图标记终点")
        } else {
            statusText.text = qsTr("测距工具已启用,请点击地图标记起点")
        }
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
                    text:               qsTr("常用工具")
                    font.pointSize:     ScreenTools.mediumFontPointSize
                    font.bold:          true
                }

                QGCButton {
                    text:               qsTr("关闭")
                    onClicked:          root.closeClicked()
                }
            }

            // 工具按钮区域
            GridLayout {
                Layout.fillWidth:   true
                columns:            3
                rowSpacing:         ScreenTools.defaultFontPixelHeight * 0.5
                columnSpacing:      ScreenTools.defaultFontPixelWidth

                QGCButton {
                    id:                 measureDistanceButton
                    Layout.fillWidth:   true
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 3
                    text:               qsTr("测距")
                    checkable:          true
                    checked:            QGroundControl.measureDistanceController.enabled

                    property bool measureDistanceEnabled: checked

                    onCheckedChanged: {
                        QGroundControl.measureDistanceController.enabled = checked
                        if (checked) {
                            console.log("测距工具已启用")
                            updateMeasureDistanceStatusText()
                        } else {
                            console.log("测距工具已禁用")
                            statusText.text = qsTr("测距工具已禁用")
                        }
                    }
                }
            }

            // 状态显示区域
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
                    text:               qsTr("常用工具状态显示栏")
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
}