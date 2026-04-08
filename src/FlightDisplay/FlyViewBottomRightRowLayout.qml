/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.FlightDisplay

RowLayout {
    property var _vehicles: QGroundControl.multiVehicleManager.vehicles
    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    // [myproject/BLACK-BOX]:一键设置当前点为home点 - 设置所有飞行器home点
    function setAllVehiclesHome() {
        for (var i = 0; i < _vehicles.count; i++) {
            var vehicle = _vehicles.get(i)
            if (vehicle && vehicle.coordinate && vehicle.coordinate.isValid) {
                vehicle.doSetHomeToVehiclePosition()
            }
        }
    }

    TelemetryValuesBar {
        Layout.alignment:       Qt.AlignBottom
        extraWidth:             instrumentPanel.extraValuesWidth
        settingsGroup:          factValueGrid.telemetryBarSettingsGroup
        specificVehicleForCard: null // Tracks active vehicle
    }

    // [myproject/BLACK-BOX]:一键设置当前点为home点 - 设置所有home按钮
    Rectangle {
        id:                 homeButton
        Layout.alignment:   Qt.AlignBottom
        width:              ScreenTools.defaultFontPixelHeight * 1.5
        height:             width
        radius:             ScreenTools.defaultFontPixelWidth / 4
        color:              statusColor
        scale:              homeMouseArea.pressed ? 0.9 : 1.0

        property color statusColor: "white"

        Behavior on scale { NumberAnimation { duration: 50 } }

        QGCLabel {
            anchors.centerIn:   parent
            text:               "H"
            color:              "black"
            font.bold:          true
            font.family:        ScreenTools.fixedFontFamily
            font.pointSize:     ScreenTools.defaultFontPointSize
        }

        QGCMouseArea {
            id:             homeMouseArea
            anchors.fill:   parent
            onClicked:      setAllVehiclesHome()
        }

        Connections {
            target:                 _activeVehicle
            enabled:                _activeVehicle !== null
            function onSetHomeResult(success) {
                if (success) {
                    homeButton.statusColor = qgcPal.colorGreen
                } else {
                    homeButton.statusColor = "red"
                }
                statusResetTimer.start()
            }
        }

        Timer {
            id:         statusResetTimer
            interval:   500
            onTriggered: homeButton.statusColor = "white"
        }
    }

    FlyViewInstrumentPanel {
        id:                 instrumentPanel
        Layout.alignment:   Qt.AlignBottom
        visible:            QGroundControl.corePlugin.options.flyView.showInstrumentPanel && _showSingleVehicleUI
    }
}
