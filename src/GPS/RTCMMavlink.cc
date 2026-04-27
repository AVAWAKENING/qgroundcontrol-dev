/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "RTCMMavlink.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"
#include <QtCore/QSet>

QGC_LOGGING_CATEGORY(RTCMMavlinkLog, "qgc.gps.rtcmmavlink")

RTCMMavlink::RTCMMavlink(QObject *parent)
    : QObject(parent)
{
    // qCDebug(RTCMMavlinkLog) << Q_FUNC_INFO << this;

    _bandwidthTimer.start();
}

RTCMMavlink::~RTCMMavlink()
{
    // qCDebug(RTCMMavlinkLog) << Q_FUNC_INFO << this;
}

void RTCMMavlink::RTCMDataUpdate(QByteArrayView data)
{
    qCDebug(RTCMMavlinkLog) << "RTCMDataUpdate called, data size:" << data.size();
#ifdef QT_DEBUG
    _calculateBandwith(data.size());
#endif

    mavlink_gps_rtcm_data_t gpsRtcmData{};

    static constexpr qsizetype maxMessageLength = MAVLINK_MSG_GPS_RTCM_DATA_FIELD_DATA_LEN;
    if (data.size() < maxMessageLength) {
        gpsRtcmData.len = data.size();
        gpsRtcmData.flags = (_sequenceId & 0x1FU) << 3;
        (void) memcpy(&gpsRtcmData.data, data.data(), data.size());
        _sendMessageToVehicle(gpsRtcmData);
    } else {
        uint8_t fragmentId = 0;
        qsizetype start = 0;
        while (start < data.size()) {
            gpsRtcmData.flags = 0x01U; // LSB set indicates message is fragmented
            gpsRtcmData.flags |= fragmentId++ << 1; // Next 2 bits are fragment id
            gpsRtcmData.flags |= (_sequenceId & 0x1FU) << 3; // Next 5 bits are sequence id

            const qsizetype length = std::min(data.size() - start, maxMessageLength);
            gpsRtcmData.len = length;

            (void) memcpy(gpsRtcmData.data, data.constData() + start, length);
            _sendMessageToVehicle(gpsRtcmData);

            start += length;
        }
    }

    ++_sequenceId;
}

void RTCMMavlink::_sendMessageToVehicle(const mavlink_gps_rtcm_data_t &data)
{
    QmlObjectListModel* const vehicles = MultiVehicleManager::instance()->vehicles();
    if (vehicles->count() == 0) {
        return;
    }

    qCDebug(RTCMMavlinkLog) << "=== RTCM Send Start ===" << "Vehicle count:" << vehicles->count();

    QSet<QString> uniqueLinkNames;
    QList<SharedLinkInterfacePtr> uniqueLinks;

    for (qsizetype i = 0; i < vehicles->count(); i++) {
        Vehicle* const vehicle = qobject_cast<Vehicle*>(vehicles->get(i));
        if (!vehicle) {
            continue;
        }

        const SharedLinkInterfacePtr sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
        if (!sharedLink) {
            continue;
        }

        QString linkName = sharedLink->linkConfiguration()->name();

        qCDebug(RTCMMavlinkLog) << "Vehicle" << i
                                 << "link:" << (void*)sharedLink.get()
                                 << "name:" << linkName
                                 << "channel:" << sharedLink->mavlinkChannel();

        if (!uniqueLinkNames.contains(linkName)) {
            uniqueLinkNames.insert(linkName);
            uniqueLinks.append(sharedLink);
            qCDebug(RTCMMavlinkLog) << "  -> Added to unique list";
        } else {
            qCDebug(RTCMMavlinkLog) << "  -> DUPLICATE detected!";
        }
    }

    qCDebug(RTCMMavlinkLog) << "Unique links count:" << uniqueLinks.count();

    for (const auto &sharedLink : uniqueLinks) {
        if (!sharedLink->isConnected()) {
            qCDebug(RTCMMavlinkLog) << "Link not connected:" << sharedLink->linkConfiguration()->name();
            continue;
        }

        mavlink_message_t message;
        (void) mavlink_msg_gps_rtcm_data_encode_chan(
            MAVLinkProtocol::instance()->getSystemId(),
            MAVLinkProtocol::getComponentId(),
            sharedLink->mavlinkChannel(),
            &message,
            &data
        );

        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        int len = mavlink_msg_to_send_buffer(buffer, &message);
        sharedLink->writeBytesThreadSafe((const char*)buffer, len);

        qCDebug(RTCMMavlinkLog) << "Sent RTCM on link:" << sharedLink->linkConfiguration()->name()
                                 << "channel:" << sharedLink->mavlinkChannel();
    }

    qCDebug(RTCMMavlinkLog) << "=== RTCM Send End ===";
}

void RTCMMavlink::_calculateBandwith(qsizetype bytes)
{
    if (!_bandwidthTimer.isValid()) {
        return;
    }

    _bandwidthByteCounter += bytes;

    const qint64 elapsed = _bandwidthTimer.elapsed();
    if (elapsed > 1000) {
        qCDebug(RTCMMavlinkLog) << QStringLiteral("RTCM bandwidth: %1 kB/s (bytes: %2, elapsed: %3 ms)")
            .arg(((_bandwidthByteCounter / static_cast<float>(elapsed)) * 1000.f) / 1024.f)
            .arg(_bandwidthByteCounter)
            .arg(elapsed);
        (void) _bandwidthTimer.restart();
        _bandwidthByteCounter = 0;
    }
}
