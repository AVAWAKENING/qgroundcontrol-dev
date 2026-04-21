/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleFactGroup.h"
#include "Vehicle.h"
#include "QGC.h"

#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>

VehicleFactGroup::VehicleFactGroup(QObject *parent)
    : FactGroup(100, QStringLiteral(":/json/Vehicle/VehicleFact.json"), parent)
{
    _addFact(&_rollFact);
    _addFact(&_pitchFact);
    _addFact(&_headingFact);
    _addFact(&_rollRateFact);
    _addFact(&_pitchRateFact);
    _addFact(&_yawRateFact);
    _addFact(&_groundSpeedFact);
    _addFact(&_airSpeedFact);
    _addFact(&_airSpeedSetpointFact);
    _addFact(&_climbRateFact);
    _addFact(&_altitudeRelativeFact);
    _addFact(&_altitudeAMSLFact);
    _addFact(&_altitudeEllipsoidFact);
    _addFact(&_altitudeAboveTerrFact);
    _addFact(&_altitudeTuningFact);
    _addFact(&_altitudeTuningSetpointFact);
    _addFact(&_xTrackErrorFact);
    _addFact(&_rangeFinderDistFact);
    _addFact(&_flightDistanceFact);
    _addFact(&_flightTimeFact);
    _addFact(&_distanceToHomeFact);
    _addFact(&_timeToHomeFact);
    _addFact(&_missionItemIndexFact);
    _addFact(&_headingToNextWPFact);
    _addFact(&_distanceToNextWPFact);
    _addFact(&_headingToHomeFact);
    _addFact(&_distanceToGCSFact);
    _addFact(&_hobbsFact);
    _addFact(&_throttlePctFact);
    _addFact(&_imuTempFact);
    _addFact(&_velocityNorthFact);

    _hobbsFact.setRawValue(QStringLiteral("0000:00:00"));
}

void VehicleFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_ATTITUDE:
        _handleAttitude(vehicle, message);
        break;
    case MAVLINK_MSG_ID_ATTITUDE_QUATERNION:
        _handleAttitudeQuaternion(vehicle, message);
        break;
    case MAVLINK_MSG_ID_ALTITUDE:
        _handleAltitude(message);
        break;
    case MAVLINK_MSG_ID_VFR_HUD:
        _handleVfrHud(message);
        break;
    case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
        _handleNavControllerOutput(message);
        break;
    case MAVLINK_MSG_ID_RAW_IMU:
        _handleRawImuTemp(message);
        break;
    case MAVLINK_MSG_ID_GNSS_LOW_BANDWIDTH_POSITION:
        _handleGnssLowBandwidthPosition(message);
        break;
#ifndef QGC_NO_ARDUPILOT_DIALECT
    case MAVLINK_MSG_ID_RANGEFINDER:
        _handleRangefinder(message);
        break;
#endif
    default:
        break;
    }
}

void VehicleFactGroup::_handleAttitudeWorker(double rollRadians, double pitchRadians, double yawRadians)
{
    double rollDegrees = QGC::limitAngleToPMPIf(rollRadians);
    double pitchDegrees = QGC::limitAngleToPMPIf(pitchRadians);
    double yawDegrees = QGC::limitAngleToPMPIf(yawRadians);

    rollDegrees = qRadiansToDegrees(rollDegrees);
    pitchDegrees = qRadiansToDegrees(pitchDegrees);
    yawDegrees = qRadiansToDegrees(yawDegrees);

    if (yawDegrees < 0.0) {
        yawDegrees += 360.0;
    }
    // truncate to integer so widget never displays 360
    yawDegrees = trunc(yawDegrees);

    roll()->setRawValue(rollDegrees);
    pitch()->setRawValue(pitchDegrees);

    // 只有当 GNSS heading 不可用时才使用 ATTITUDE 消息的 heading
    if (!_gnssLowBandwidthHeadingAvailable) {
        heading()->setRawValue(yawDegrees);
    }
}

void VehicleFactGroup::_handleAttitude(Vehicle *vehicle, const mavlink_message_t &message)
{
    if ((message.sysid != vehicle->id()) || (message.compid != vehicle->compId())) {
        return;
    }

    if (_receivingAttitudeQuaternion) {
        return;
    }

    mavlink_attitude_t attitude{};
    mavlink_msg_attitude_decode(&message, &attitude);

    _handleAttitudeWorker(attitude.roll, attitude.pitch, attitude.yaw);

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleAltitude(const mavlink_message_t &message)
{
    mavlink_altitude_t altitude{};
    mavlink_msg_altitude_decode(&message, &altitude);

    // Data from ALTITUDE message takes precedence over gps messages
    _altitudeMessageAvailable = true;
    altitudeRelative()->setRawValue(altitude.altitude_relative);
    altitudeAMSL()->setRawValue(altitude.altitude_amsl);

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleAttitudeQuaternion(Vehicle *vehicle, const mavlink_message_t &message)
{
    // only accept the attitude message from the vehicle's flight controller
    if ((message.sysid != vehicle->id()) || (message.compid != vehicle->compId())) {
        return;
    }

    _receivingAttitudeQuaternion = true;

    mavlink_attitude_quaternion_t attitudeQuaternion{};
    mavlink_msg_attitude_quaternion_decode(&message, &attitudeQuaternion);

    QQuaternion quat(attitudeQuaternion.q1, attitudeQuaternion.q2, attitudeQuaternion.q3, attitudeQuaternion.q4);
    QVector3D rates(attitudeQuaternion.rollspeed, attitudeQuaternion.pitchspeed, attitudeQuaternion.yawspeed);
    QQuaternion repr_offset(attitudeQuaternion.repr_offset_q[0], attitudeQuaternion.repr_offset_q[1], attitudeQuaternion.repr_offset_q[2], attitudeQuaternion.repr_offset_q[3]);
    QQuaternion reprOffsetQuat = repr_offset;

    QQuaternion attitudeQuaternionFinal = quat * reprOffsetQuat;
    attitudeQuaternionFinal.normalize();

    QVector3D euler = attitudeQuaternionFinal.toEulerAngles();

    _handleAttitudeWorker(euler.x(), euler.y(), euler.z());

    rollRate()->setRawValue(rates.x());
    pitchRate()->setRawValue(rates.y());
    yawRate()->setRawValue(rates.z());

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleGnssLowBandwidthPosition(const mavlink_message_t &message)
{
    mavlink_gnss_low_bandwidth_position_t gnssLowBandwidth{};
    mavlink_msg_gnss_low_bandwidth_position_decode(&message, &gnssLowBandwidth);

    // 使用 GNSS_LOW_BANDWIDTH_POSITION 中的 heading（单位：度/100）
    double headingDegrees = gnssLowBandwidth.heading / 100.0;
    heading()->setRawValue(headingDegrees);
    _gnssLowBandwidthHeadingAvailable = true;

    altitudeEllipsoid()->setRawValue(gnssLowBandwidth.altitude_ellipsoid_mm / 1000.0);

    // 修改 1: groundSpeed 现在使用 vn（地速/北向速度分量），单位从 cm/s 转换为 m/s
    groundSpeed()->setRawValue(gnssLowBandwidth.vn / 100.0);

    // 修改 2: climbRate 现在使用 ve（垂直速度/东向速度分量），单位从 cm/s 转换为 m/s
    climbRate()->setRawValue(gnssLowBandwidth.ve / 100.0);

    // 修改 3: velocityNorth 使用 vd（NED 北向速度/垂直向下速度），单位从 cm/s 转换为 m/s
    velocityNorth()->setRawValue(gnssLowBandwidth.vd / 100.0);

    _gnssLowBandwidthSpeedAvailable = true;

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleNavControllerOutput(const mavlink_message_t &message)
{
    mavlink_nav_controller_output_t navControllerOutput{};
    mavlink_msg_nav_controller_output_decode(&message, &navControllerOutput);

    altitudeTuningSetpoint()->setRawValue(_altitudeTuningFact.rawValue().toDouble() - navControllerOutput.alt_error);
    xTrackError()->setRawValue(navControllerOutput.xtrack_error);
    airSpeedSetpoint()->setRawValue(_airSpeedFact.rawValue().toDouble() - navControllerOutput.aspd_error);
    distanceToNextWP()->setRawValue(navControllerOutput.wp_dist);

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleVfrHud(const mavlink_message_t &message)
{
    mavlink_vfr_hud_t vfrHud{};
    mavlink_msg_vfr_hud_decode(&message, &vfrHud);

    airSpeed()->setRawValue(qIsNaN(vfrHud.airspeed) ? 0 : vfrHud.airspeed);

    // 只有当 GNSS 速度不可用时才使用 VFR_HUD 的地速
    if (!_gnssLowBandwidthSpeedAvailable) {
        groundSpeed()->setRawValue(qIsNaN(vfrHud.groundspeed) ? 0 : vfrHud.groundspeed);
    }

    climbRate()->setRawValue(qIsNaN(vfrHud.climb) ? 0 : vfrHud.climb);
    throttlePct()->setRawValue(static_cast<int16_t>(vfrHud.throttle));
    if (qIsNaN(_altitudeTuningOffset)) {
        _altitudeTuningOffset = vfrHud.alt;
    }
    altitudeTuning()->setRawValue(vfrHud.alt - _altitudeTuningOffset);
    if (!qIsNaN(vfrHud.groundspeed) && !qIsNaN(_distanceToHomeFact.cookedValue().toDouble())) {
      timeToHome()->setRawValue(_distanceToHomeFact.cookedValue().toDouble() / vfrHud.groundspeed);
    }

    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleRawImuTemp(const mavlink_message_t &message)
{
    mavlink_raw_imu_t imuRaw{};
    mavlink_msg_raw_imu_decode(&message, &imuRaw);

    imuTemp()->setRawValue((imuRaw.temperature == 0) ? 0 : (imuRaw.temperature * 0.01));

    _setTelemetryAvailable(true);
}

#ifndef QGC_NO_ARDUPILOT_DIALECT
void VehicleFactGroup::_handleRangefinder(const mavlink_message_t &message)
{
    mavlink_rangefinder_t rangefinder{};
    mavlink_msg_rangefinder_decode(&message, &rangefinder);

    rangeFinderDist()->setRawValue(qIsNaN(rangefinder.distance) ? 0 : rangefinder.distance);

    _setTelemetryAvailable(true);
}
#endif
