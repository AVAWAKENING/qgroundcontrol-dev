/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DataForwardingSender.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"

#include <QtCore/QThread>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QUdpSocket>
#include <QtPositioning/QGeoCoordinate>
#include <QtCore/QElapsedTimer>
#include <QtMath>
#include <algorithm>

QGC_LOGGING_CATEGORY(DataForwardingSenderLog, "qgc.comms.dataforwardingsender")

namespace {
    constexpr double WGS84_A = 6378137.0;
    constexpr double WGS84_B = 6356752.3142;
    constexpr double WGS84_E2 = (WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_A * WGS84_A);
    
    constexpr int16_t FLAG_VALUE = 0x22EB;
    constexpr uint8_t DATA_SOURCE_INTERNAL = 0;
}

DataForwardingWorker::DataForwardingWorker(QObject *parent)
    : QObject(parent)
    , _socket(new QUdpSocket(this))
    , _timer(new QTimer(this))
    , _isRunning(false)
    , _targetPort(0)
    , _frequencyHz(0)
{
    _socket->setProxy(QNetworkProxy::NoProxy);

    (void) connect(_timer, &QTimer::timeout, this, &DataForwardingWorker::_onTimeout);
    (void) connect(_socket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError)
        emit errorOccurred(_socket->errorString());
    });
}

DataForwardingWorker::~DataForwardingWorker()
{
    stopForwarding();
}

void DataForwardingWorker::startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                           double originLat, double originLon, double originAlt)
{
    if (_isRunning) {
        qCWarning(DataForwardingSenderLog) << "Already running";
        return;
    }

    _targetAddress = QHostAddress(ip);
    if (_targetAddress.isNull()) {
        emit errorOccurred(QStringLiteral("Invalid IP address: %1").arg(ip));
        return;
    }

    _targetPort = port;
    _frequencyHz = frequencyHz;
    _originLat = originLat;
    _originLon = originLon;
    _originAlt = originAlt;

    qCDebug(DataForwardingSenderLog) << "Starting forwarding to" << ip << ":" << port 
                                     << "at" << frequencyHz << "Hz"
                                     << "Origin:" << originLat << originLon << originAlt;

    _updateVehicleList();
    
    int intervalMs = static_cast<int>(1000.0 / _frequencyHz);
    if (intervalMs > 0) {
        _timer->start(intervalMs);
    } else {
        qCWarning(DataForwardingSenderLog) << "Frequency too high, using minimum interval";
        _timer->start(1);
    }
    _isRunning = true;

    emit forwardingStarted();
}

void DataForwardingWorker::stopForwarding()
{
    if (!_isRunning) {
        return;
    }

    _timer->stop();
    _isRunning = false;
    
    for (Vehicle* vehicle : _vehicleList) {
        disconnect(vehicle, &Vehicle::coordinateChanged, this, &DataForwardingWorker::_onVehicleCoordinateChanged);
    }
    _vehicleList.clear();

    qCDebug(DataForwardingSenderLog) << "Forwarding stopped";

    emit forwardingStopped();
}

void DataForwardingWorker::sendData(const QByteArray &data)
{
    if (!_isRunning) {
        emit errorOccurred(QStringLiteral("Forwarding not started"));
        return;
    }

    qint64 bytesSent = _socket->writeDatagram(data, _targetAddress, _targetPort);
    if (bytesSent < 0) {
        emit errorOccurred(QStringLiteral("Failed to send data: %1").arg(_socket->errorString()));
        return;
    }

    qCDebug(DataForwardingSenderLog) << "Sent" << bytesSent << "bytes to" 
                                     << _targetAddress.toString() << ":" << _targetPort;
    emit dataSent(data);
}

void DataForwardingWorker::_onTimeout()
{
    if (!_isRunning) {
        return;
    }

    QByteArray packet = _buildPacket();
    if (!packet.isEmpty()) {
        sendData(packet);
    }
}

void DataForwardingWorker::_updateVehicleList()
{
    for (Vehicle* vehicle : _vehicleList) {
        disconnect(vehicle, &Vehicle::coordinateChanged, this, &DataForwardingWorker::_onVehicleCoordinateChanged);
    }
    _vehicleList.clear();

    MultiVehicleManager* multiVehicleManager = MultiVehicleManager::instance();
    if (!multiVehicleManager) {
        qCWarning(DataForwardingSenderLog) << "MultiVehicleManager instance not available";
        return;
    }

    QmlObjectListModel* vehicles = multiVehicleManager->vehicles();
    if (!vehicles) {
        return;
    }

    for (int i = 0; i < vehicles->count(); i++) {
        Vehicle* vehicle = qobject_cast<Vehicle*>(vehicles->get(i));
        if (vehicle && vehicle->coordinate().isValid()) {
            _vehicleList.append(vehicle);
            connect(vehicle, &Vehicle::coordinateChanged, this, &DataForwardingWorker::_onVehicleCoordinateChanged,
                    Qt::QueuedConnection);
        }
    }

    std::sort(_vehicleList.begin(), _vehicleList.end(), [](Vehicle* a, Vehicle* b) {
        return a->id() < b->id();
    });

    qCDebug(DataForwardingSenderLog) << "Vehicle list updated:" << _vehicleList.size() << "vehicles";
}

void DataForwardingWorker::_onVehicleCoordinateChanged()
{
}

int32_t DataForwardingWorker::_convertToEastUpSouth(const QGeoCoordinate &vehicleCoord,
                                                     double originLat, double originLon, double originAlt,
                                                     char axis)
{
    double latRad = vehicleCoord.latitude() * M_PI / 180.0;
    double lonRad = vehicleCoord.longitude() * M_PI / 180.0;
    double altM = vehicleCoord.altitude();

    double originLatRad = originLat * M_PI / 180.0;
    double originLonRad = originLon * M_PI / 180.0;
    double originAltM = originAlt;

    double N = WGS84_A / qSqrt(1.0 - WGS84_E2 * qSin(latRad) * qSin(latRad));
    double X = (N + altM) * qCos(latRad) * qCos(lonRad);
    double Y = (N + altM) * qCos(latRad) * qSin(lonRad);
    double Z = (N * (1.0 - WGS84_E2) + altM) * qSin(latRad);

    double N0 = WGS84_A / qSqrt(1.0 - WGS84_E2 * qSin(originLatRad) * qSin(originLatRad));
    double X0 = (N0 + originAltM) * qCos(originLatRad) * qCos(originLonRad);
    double Y0 = (N0 + originAltM) * qCos(originLatRad) * qSin(originLonRad);
    double Z0 = (N0 * (1.0 - WGS84_E2) + originAltM) * qSin(originLatRad);

    double dX = X - X0;
    double dY = Y - Y0;
    double dZ = Z - Z0;

    double result = 0.0;
    switch (axis) {
    case 'x':
        result = -qSin(originLonRad) * dX + qCos(originLonRad) * dY;
        break;
    case 'y':
        result = qCos(originLatRad) * qCos(originLonRad) * dX + 
                 qCos(originLatRad) * qSin(originLonRad) * dY + 
                 qSin(originLatRad) * dZ;
        break;
    case 'z':
        result = qSin(originLatRad) * qCos(originLonRad) * dX + 
                 qSin(originLatRad) * qSin(originLonRad) * dY - 
                 qCos(originLatRad) * dZ;
        break;
    }

    return static_cast<int32_t>(result * 8.0);
}

QByteArray DataForwardingWorker::_buildPacket()
{
    if (_vehicleList.isEmpty()) {
        return QByteArray();
    }

    QByteArray packet;
    
    int16_t flag = FLAG_VALUE;
    packet.append(reinterpret_cast<char*>(&flag), 2);
    
    int32_t time = -1;
    packet.append(reinterpret_cast<char*>(&time), 4);
    
    int16_t channelNum = static_cast<int16_t>(_vehicleList.size());
    packet.append(reinterpret_cast<char*>(&channelNum), 2);
    
    int16_t deviceId = 0;
    packet.append(reinterpret_cast<char*>(&deviceId), 2);

    for (Vehicle* vehicle : _vehicleList) {
        QGeoCoordinate coord = vehicle->coordinate();
        if (!coord.isValid()) {
            continue;
        }

        int16_t src = 0;
        int16_t vehicleId = static_cast<int16_t>(vehicle->id());
        src = (vehicleId & 0x0FFF);
        src |= ((DATA_SOURCE_INTERNAL & 0x03) << 12);
        src &= 0x7FFF;
        packet.append(reinterpret_cast<char*>(&src), 2);

        int32_t x = _convertToEastUpSouth(coord, _originLat, _originLon, _originAlt, 'x');
        packet.append(reinterpret_cast<char*>(&x), 4);

        int32_t y = _convertToEastUpSouth(coord, _originLat, _originLon, _originAlt, 'y');
        packet.append(reinterpret_cast<char*>(&y), 4);

        int32_t z = _convertToEastUpSouth(coord, _originLat, _originLon, _originAlt, 'z');
        packet.append(reinterpret_cast<char*>(&z), 4);

        int32_t vx = -1;
        packet.append(reinterpret_cast<char*>(&vx), 4);

        int32_t vy = -1;
        packet.append(reinterpret_cast<char*>(&vy), 4);

        int32_t vz = -1;
        packet.append(reinterpret_cast<char*>(&vz), 4);

        int32_t v = -1;
        packet.append(reinterpret_cast<char*>(&v), 4);

        int32_t rcs = -1;
        packet.append(reinterpret_cast<char*>(&rcs), 4);

        int32_t reserved = -1;
        packet.append(reinterpret_cast<char*>(&reserved), 4);
    }

    return packet;
}

DataForwardingSender::DataForwardingSender(QObject *parent)
    : QObject(parent)
    , _worker(new DataForwardingWorker())
    , _workerThread(new QThread(this))
{
    _workerThread->setObjectName(QStringLiteral("DataForwardingThread"));

    _worker->moveToThread(_workerThread);

    (void) connect(_workerThread, &QThread::finished, _worker, &QObject::deleteLater);

    (void) connect(_worker, &DataForwardingWorker::forwardingStarted, this, &DataForwardingSender::forwardingStarted, Qt::QueuedConnection);
    (void) connect(_worker, &DataForwardingWorker::forwardingStopped, this, &DataForwardingSender::forwardingStopped, Qt::QueuedConnection);
    (void) connect(_worker, &DataForwardingWorker::errorOccurred, this, &DataForwardingSender::errorOccurred, Qt::QueuedConnection);
    (void) connect(_worker, &DataForwardingWorker::dataSent, this, &DataForwardingSender::dataSent, Qt::QueuedConnection);

    _workerThread->start();
}

DataForwardingSender::~DataForwardingSender()
{
    stopForwarding();

    _workerThread->quit();
    if (!_workerThread->wait(1000)) {
        qCWarning(DataForwardingSenderLog) << "Failed to wait for worker thread to close";
    }
}

void DataForwardingSender::startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                           double originLat, double originLon, double originAlt)
{
    (void) QMetaObject::invokeMethod(_worker, "startForwarding", Qt::QueuedConnection,
                                     Q_ARG(QString, ip), Q_ARG(quint16, port), Q_ARG(double, frequencyHz),
                                     Q_ARG(double, originLat), Q_ARG(double, originLon), Q_ARG(double, originAlt));
}

void DataForwardingSender::stopForwarding()
{
    (void) QMetaObject::invokeMethod(_worker, "stopForwarding", Qt::QueuedConnection);
}

void DataForwardingSender::sendData(const QByteArray &data)
{
    (void) QMetaObject::invokeMethod(_worker, "sendData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
}
