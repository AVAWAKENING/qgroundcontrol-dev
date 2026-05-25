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
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

Rectangle {
    id:                 root
    implicitWidth:      ScreenTools.defaultFontPixelWidth * 45
    implicitHeight:     ScreenTools.defaultFontPixelHeight * 18
    color:              qgcPal.windowShade
    radius:             ScreenTools.defaultFontPixelWidth / 2

    property var popupParent: null
    signal closeClicked()

    QGCPalette { id: qgcPal }

    Settings {
        id: settings
        property string manualLatitude:    "41.0774773"
        property string manualLongitude:   "100.5217878"
        property string manualAltitude:    "1046.505"
        property bool   manualPositionEnabled: false
    }

    ColumnLayout {
        anchors.margins:    ScreenTools.defaultFontPixelHeight * 0.3
        anchors.fill:       parent
        spacing:            ScreenTools.defaultFontPixelHeight * 0.3

        RowLayout {
            Layout.fillWidth: true
            QGCLabel {
                Layout.fillWidth:   true
                text:               qsTr("GCS坐标设置")
                font.bold:          true
            }
            QGCButton {
                text:   qsTr("×")
                width:  ScreenTools.defaultFontPixelHeight * 1.5
                height: width
                onClicked: root.closeClicked()
            }
        }

        Rectangle {
            Layout.fillWidth:   true
            height:             1
            color:              qgcPal.windowShadeLight
        }

        RowLayout {
            Layout.fillWidth: true
            QGCLabel { text: qsTr("手动设置") }
            Switch {
                id: manualSwitch
                checked: settings.manualPositionEnabled
                onCheckedChanged: {
                    settings.manualPositionEnabled = checked
                    if (checked) applyManualPosition()
                    QGroundControl.qgcPositionManger.manualPositionEnabled = checked
                }
            }
        }

        GridLayout {
            Layout.fillWidth:   true
            columns:            2
            rowSpacing:         ScreenTools.defaultFontPixelHeight * 0.15
            columnSpacing:      ScreenTools.defaultFontPixelWidth

            QGCLabel { text: qsTr("纬度 (°):") }
            QGCTextField {
                Layout.fillWidth:   true
                text:               settings.manualLatitude
                onEditingFinished: {
                    settings.manualLatitude = text
                    if (manualSwitch.checked) applyManualPosition()
                }
            }

            QGCLabel { text: qsTr("经度 (°):") }
            QGCTextField {
                Layout.fillWidth:   true
                text:               settings.manualLongitude
                onEditingFinished: {
                    settings.manualLongitude = text
                    if (manualSwitch.checked) applyManualPosition()
                }
            }

            QGCLabel { text: qsTr("海拔高度 (m):") }
            QGCTextField {
                Layout.fillWidth:   true
                text:               settings.manualAltitude
                onEditingFinished: {
                    settings.manualAltitude = text
                    if (manualSwitch.checked) applyManualPosition()
                }
            }
        }

        QGCLabel {
            id:                 statusText
            Layout.fillWidth:   true
            text:               qsTr("就绪")
            wrapMode:           Text.WordWrap
        }
    }

    function applyManualPosition() {
        var lat = parseFloat(settings.manualLatitude)
        var lon = parseFloat(settings.manualLongitude)
        var alt = parseFloat(settings.manualAltitude)

        if (isNaN(lat) || lat < -90 || lat > 90) {
            statusText.text = qsTr("错误：纬度必须在-90到90之间")
            return
        }
        if (isNaN(lon) || lon < -180 || lon > 180) {
            statusText.text = qsTr("错误：经度必须在-180到180之间")
            return
        }
        if (isNaN(alt)) {
            statusText.text = qsTr("错误：高度必须是有效数字")
            return
        }

        var coord = QtPositioning.coordinate(lat, lon, alt)
        if (!coord.isValid) {
            statusText.text = qsTr("错误：坐标无效")
            return
        }

        QGroundControl.qgcPositionManger.manualPosition = coord
        statusText.text = qsTr("已应用: ") + lat.toFixed(7) + ", " + lon.toFixed(7)
    }
}
