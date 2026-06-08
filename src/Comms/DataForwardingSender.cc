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
#include "VehicleGPSFactGroup.h"
#include "VehicleLocalPositionFactGroup.h"
#include "VehicleFactGroup.h"
#include "QGCApplication.h"

#include <QtCore/QThread>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QUdpSocket>
#include <QtPositioning/QGeoCoordinate>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>
#include <QtMath>
#include <algorithm>
#include <QtCore/qapplicationstatic.h>
#include <QtQml/QQmlEngine>

QGC_LOGGING_CATEGORY(DataForwardingSenderLog, "qgc.comms.dataforwardingsender")

Q_APPLICATION_STATIC(DataForwardingSender, _dataForwardingSenderInstance);

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
    _elapsedTimer.start();
}

void DataForwardingWorker::initialize()
{
    (void) connect(_timer, &QTimer::timeout, this, &DataForwardingWorker::_onTimeout);
    (void) connect(_socket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError)
        emit errorOccurred(_socket->errorString());
    });

    MultiVehicleManager* multiVehicleManager = MultiVehicleManager::instance();
    if (multiVehicleManager) {
        (void) connect(multiVehicleManager, &MultiVehicleManager::vehicleAdded,
                       this, &DataForwardingWorker::_onVehicleAdded, Qt::QueuedConnection);
        (void) connect(multiVehicleManager, &MultiVehicleManager::vehicleRemoved,
                       this, &DataForwardingWorker::_onVehicleRemoved, Qt::QueuedConnection);
        qCDebug(DataForwardingSenderLog) << "Connected to MultiVehicleManager signals";
    } else {
        qCWarning(DataForwardingSenderLog) << "MultiVehicleManager instance not available";
    }
}

DataForwardingWorker::~DataForwardingWorker()
{
    stopForwarding();
}

void DataForwardingWorker::startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                           double originLat, double originLon, double originAlt, int radarId, int deviceId)
{
    if (_isRunning) {
        qCWarning(DataForwardingSenderLog) << "Already running";
        return;
    }

    if (frequencyHz <= 0.0) {
        emit errorOccurred(QStringLiteral("Frequency must be positive"));
        return;
    }

    if (frequencyHz > 100.0) {
        emit errorOccurred(QStringLiteral("Frequency too high (max 100Hz)"));
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
    _radarId = radarId;
    _deviceId = deviceId;

    qCDebug(DataForwardingSenderLog) << "Starting forwarding to" << ip << ":" << port
                                     << "at" << frequencyHz << "Hz"
                                     << "Origin:" << originLat << originLon << originAlt
                                     << "Radar ID:" << radarId
                                     << "Device ID:" << deviceId;

    _updateVehicleList();

    _elapsedTimer.restart();
    _lastSendTime = 0;
    _actualSendCount = 0;
    _expectedSendCount = 0;

    int intervalMs = qMax(1, static_cast<int>(1000.0 / _frequencyHz / 2));
    _timer->start(intervalMs);
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

    // 线程安全地清理车辆列表
    {
        QMutexLocker locker(&_vehicleListMutex);
        _vehicleList.clear();
    }

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

    qint64 currentTime = _elapsedTimer.elapsed();
    qint64 targetInterval = static_cast<qint64>(1000.0 / _frequencyHz);
    qint64 expectedTime = _expectedSendCount * targetInterval;

    if (currentTime >= expectedTime) {
        QByteArray packet = _buildPacket();
        if (!packet.isEmpty()) {
            sendData(packet);
            _actualSendCount++;
        }
        _expectedSendCount++;

        if (_expectedSendCount % 100 == 0) {
            double actualFreq = _actualSendCount * 1000.0 / currentTime;
            qCDebug(DataForwardingSenderLog) << "Frequency stats - Target:" << _frequencyHz 
                                             << "Hz, Actual:" << actualFreq << "Hz"
                                             << "Count:" << _actualSendCount << "/" << _expectedSendCount;
            emit frequencyStatsUpdated(actualFreq, _actualSendCount);
        }
    }
}

void DataForwardingWorker::_updateVehicleList()
{
    QMutexLocker locker(&_vehicleListMutex);  // 线程安全保护

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
            // 使用 QPointer 包装，防止悬空指针
            _vehicleList.append(QPointer<Vehicle>(vehicle));
        }
    }

    // 过滤掉无效的 QPointer
    _vehicleList.erase(std::remove_if(_vehicleList.begin(), _vehicleList.end(),
        [](const QPointer<Vehicle>& ptr) { return !ptr; }),
        _vehicleList.end());

    std::sort(_vehicleList.begin(), _vehicleList.end(), [](const QPointer<Vehicle>& a, const QPointer<Vehicle>& b) {
        if (!a || !b) {
            return static_cast<bool>(a);
        }
        return a->id() < b->id();
    });

    qCDebug(DataForwardingSenderLog) << "Vehicle list updated:" << _vehicleList.size() << "vehicles";
}

void DataForwardingWorker::_onVehicleAdded(Vehicle* vehicle)
{
    if (!vehicle) {
        return;
    }

    QMutexLocker locker(&_vehicleListMutex);

    if (!vehicle->coordinate().isValid()) {
        qCDebug(DataForwardingSenderLog) << "Vehicle added but coordinate not valid, skipping:" << vehicle->id();
        return;
    }

    for (const QPointer<Vehicle>& ptr : _vehicleList) {
        if (ptr && ptr.data() == vehicle) {
            qCDebug(DataForwardingSenderLog) << "Vehicle already in list:" << vehicle->id();
            return;
        }
    }

    _vehicleList.append(QPointer<Vehicle>(vehicle));

    std::sort(_vehicleList.begin(), _vehicleList.end(), [](const QPointer<Vehicle>& a, const QPointer<Vehicle>& b) {
        if (!a || !b) {
            return static_cast<bool>(a);
        }
        return a->id() < b->id();
    });

    qCDebug(DataForwardingSenderLog) << "Vehicle added to forwarding list:" << vehicle->id()
                                     << "Total vehicles:" << _vehicleList.size();
}

void DataForwardingWorker::_onVehicleRemoved(Vehicle* vehicle)
{
    if (!vehicle) {
        return;
    }

    QMutexLocker locker(&_vehicleListMutex);

    auto it = std::find_if(_vehicleList.begin(), _vehicleList.end(),
        [vehicle](const QPointer<Vehicle>& ptr) {
            return ptr && ptr.data() == vehicle;
        });

    if (it != _vehicleList.end()) {
        _vehicleList.erase(it);
        qCDebug(DataForwardingSenderLog) << "Vehicle removed from forwarding list:" << vehicle->id()
                                         << "Remaining vehicles:" << _vehicleList.size();
    }
}

DataForwardingWorker::ENUCoordinates DataForwardingWorker::_convertToENU(const QGeoCoordinate &vehicleCoord,
                                                                           double originLat, double originLon, double originAlt,
                                                                           double altitudeEllipsoid)
{
    double latRad = vehicleCoord.latitude() * M_PI / 180.0;
    double lonRad = vehicleCoord.longitude() * M_PI / 180.0;
    double altM = altitudeEllipsoid;

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

    // 转换为本地坐标系 单位1/8米
    double local_x = (-qSin(originLonRad) * dX + qCos(originLonRad) * dY) * 8.0;
    double local_y = (qCos(originLatRad) * qCos(originLonRad) * dX +
                     qCos(originLatRad) * qSin(originLonRad) * dY +
                     qSin(originLatRad) * dZ) * 8.0;
    double local_z = (qSin(originLatRad) * qCos(originLonRad) * dX +
                     qSin(originLatRad) * qSin(originLonRad) * dY -
                     qCos(originLatRad) * dZ) * 8.0;

    ENUCoordinates result;
    result.east = static_cast<int32_t>(local_x);
    result.up = static_cast<int32_t>(local_y);
    result.south = static_cast<int32_t>(local_z);

    return result;
}

QByteArray DataForwardingWorker::_buildPacket()
{
    QMutexLocker locker(&_vehicleListMutex);  // 线程安全保护

    if (_vehicleList.isEmpty()) {
        return QByteArray();
    }

    QByteArray packet;

    static const int headerSize = 10;
    static const int perVehicleSize = 40;
    int maxSize = headerSize + _vehicleList.size() * perVehicleSize;
    packet.reserve(maxSize);

    int16_t flag = FLAG_VALUE;
    packet.append(reinterpret_cast<char*>(&flag), 2);

    // 计算自本周一 0 时 0 分 0 秒起的毫秒数
    QDateTime now = QDateTime::currentDateTime();
    QDate mondayDate = now.date().addDays(-(now.date().dayOfWeek() - 1));
    QDateTime mondayStart(mondayDate, QTime(0, 0, 0));
    int32_t time = static_cast<int32_t>(mondayStart.msecsTo(now));
    packet.append(reinterpret_cast<char*>(&time), 4);

    int16_t channelNum = static_cast<int16_t>(_vehicleList.size());
    packet.append(reinterpret_cast<char*>(&channelNum), 2);

    int16_t deviceId = static_cast<int16_t>(_deviceId);
    packet.append(reinterpret_cast<char*>(&deviceId), 2);

    for (const QPointer<Vehicle>& vehiclePtr : _vehicleList) {
        Vehicle* vehicle = vehiclePtr.data();
        if (!vehicle) {
            continue;
        }

        try {
            QGeoCoordinate coord = vehicle->coordinate();
            if (!coord.isValid()) {
                continue;
            }

            uint16_t src = 0;
            uint16_t vehicleId = static_cast<uint16_t>(vehicle->id());

            int gpsFixType = 0;
            VehicleGPSFactGroup* gpsFactGroup = qobject_cast<VehicleGPSFactGroup*>(vehicle->gpsFactGroup());
            if (gpsFactGroup) {
                Fact* lockFact = gpsFactGroup->lock();
                if (lockFact) {
                    gpsFixType = lockFact->rawValue().toInt();
                }
            }

            src = (vehicleId & 0x0FFF);

            src |= ((_radarId & 0x07) << 12);

            if (gpsFixType < 3) {
                src |= (1 << 15);
            } else {
                src &= ~(1 << 15);
            }

            packet.append(reinterpret_cast<char*>(&src), 2);

            VehicleFactGroup* vehicleFactGroup =
                qobject_cast<VehicleFactGroup*>(vehicle->vehicleFactGroup());

            double altitudeEllipsoid = qQNaN();
            if (vehicleFactGroup) {
                Fact* altEllipsoidFact = vehicleFactGroup->altitudeEllipsoid();
                if (altEllipsoidFact && altEllipsoidFact->rawValue().isValid()) {
                    altitudeEllipsoid = altEllipsoidFact->rawValue().toDouble();
                }
            }

            ENUCoordinates enu = _convertToENU(coord, _originLat, _originLon, _originAlt, altitudeEllipsoid);
            packet.append(reinterpret_cast<char*>(&enu.east), 4);
            packet.append(reinterpret_cast<char*>(&enu.up), 4);
            packet.append(reinterpret_cast<char*>(&enu.south), 4);

            VehicleLocalPositionFactGroup* localPosFactGroup =
                qobject_cast<VehicleLocalPositionFactGroup*>(vehicle->localPositionFactGroup());

            int32_t v_ground = 0;
            int32_t v_up = 0;
            int32_t v_south = 0;
            int32_t v = 0;

            if (localPosFactGroup && vehicleFactGroup) {
                Fact* vxFact = localPosFactGroup->vx();
                Fact* groundSpeedFact = vehicleFactGroup->groundSpeed();
                Fact* climbRateFact = vehicleFactGroup->climbRate();

                double vxVal = 0.0;
                double groundSpeedVal = 0.0;
                double climbRateVal = 0.0;
                bool hasAllSpeeds = true;

                if (vxFact && vxFact->rawValue().isValid()) {
                    vxVal = vxFact->rawValue().toDouble();
                } else {
                    hasAllSpeeds = false;
                }

                if (groundSpeedFact && groundSpeedFact->rawValue().isValid()) {
                    groundSpeedVal = groundSpeedFact->rawValue().toDouble();
                } else {
                    hasAllSpeeds = false;
                }

                if (climbRateFact && climbRateFact->rawValue().isValid()) {
                    climbRateVal = climbRateFact->rawValue().toDouble();
                } else {
                    hasAllSpeeds = false;
                }

                if (hasAllSpeeds) {
                    v_ground = static_cast<int32_t>(groundSpeedVal * 1024.0);
                    v_up = static_cast<int32_t>(climbRateVal * 1024.0);
                    v_south = static_cast<int32_t>(-vxVal * 1024.0);
                    // v = static_cast<int32_t>(qSqrt(v_ground*v_ground + v_up*v_up + v_south*v_south));
                    // 根据前方要求，v 应该是地面速度
                    v = static_cast<int32_t>(groundSpeedVal * 1024.0);
                }
            }

            packet.append(reinterpret_cast<char*>(&v_ground), 4);
            packet.append(reinterpret_cast<char*>(&v_up), 4);
            packet.append(reinterpret_cast<char*>(&v_south), 4);
            packet.append(reinterpret_cast<char*>(&v), 4);

            int32_t rcs = -1;
            packet.append(reinterpret_cast<char*>(&rcs), 4);

            int32_t reserved = -1;
            packet.append(reinterpret_cast<char*>(&reserved), 4);
        } catch (const std::exception& e) {
            qCWarning(DataForwardingSenderLog) << "Exception while processing vehicle" << vehicle->id() << ":" << e.what();
            continue;
        } catch (...) {
            qCWarning(DataForwardingSenderLog) << "Unknown exception while processing vehicle" << vehicle->id();
            continue;
        }
    }

    return packet;
}

DataForwardingSender::DataForwardingSender(QObject *parent)
    : QObject(parent)
    , _worker(nullptr)
    , _workerThread(nullptr)
{
    // 懒初始化：在第一次使用时创建 worker 和线程
}

DataForwardingSender::~DataForwardingSender()
{
    if (_workerThread && _workerThread->isRunning()) {
        // 确保 worker 停止
        (void) QMetaObject::invokeMethod(_worker, "stopForwarding", Qt::BlockingQueuedConnection);

        _workerThread->quit();
        if (!_workerThread->wait(1000)) {
            qCWarning(DataForwardingSenderLog) << "Failed to wait for worker thread to close";
        }
    }

    qCDebug(DataForwardingSenderLog) << "~DataForwardingSender" << this;
}

void DataForwardingSender::ensureInitialized()
{
    if (!_workerThread) {
        // 懒初始化：第一次使用时创建 worker 和线程
        _worker = new DataForwardingWorker();
        _workerThread = new QThread(this);

        _workerThread->setObjectName(QStringLiteral("DataForwardingThread"));

        _worker->moveToThread(_workerThread);

        (void) connect(_workerThread, &QThread::started, _worker, &DataForwardingWorker::initialize);
        (void) connect(_workerThread, &QThread::finished, _worker, &QObject::deleteLater);

        (void) connect(_worker, &DataForwardingWorker::forwardingStarted, this, &DataForwardingSender::forwardingStarted, Qt::QueuedConnection);
        (void) connect(_worker, &DataForwardingWorker::forwardingStopped, this, &DataForwardingSender::forwardingStopped, Qt::QueuedConnection);
        (void) connect(_worker, &DataForwardingWorker::errorOccurred, this, &DataForwardingSender::errorOccurred, Qt::QueuedConnection);
        (void) connect(_worker, &DataForwardingWorker::dataSent, this, &DataForwardingSender::dataSent, Qt::QueuedConnection);
        (void) connect(_worker, &DataForwardingWorker::frequencyStatsUpdated, this, &DataForwardingSender::frequencyStatsUpdated, Qt::QueuedConnection);

        _workerThread->start();

        if (!_workerThread->isRunning()) {
            qCCritical(DataForwardingSenderLog) << "Failed to start worker thread";
            _worker->deleteLater();
            _worker = nullptr;
            delete _workerThread;
            _workerThread = nullptr;
            return;
        }

        qCDebug(DataForwardingSenderLog) << "DataForwardingSender initialized with worker thread";
    }
}

DataForwardingSender *DataForwardingSender::instance()
{
    return _dataForwardingSenderInstance();
}

void DataForwardingSender::registerQmlTypes()
{
    (void) qmlRegisterSingletonType<DataForwardingSender>("QGroundControl.Comms", 1, 0, "DataForwardingSender",
        [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
            Q_UNUSED(engine)
            Q_UNUSED(scriptEngine)
            return DataForwardingSender::instance();
        });
}

void DataForwardingSender::startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                           double originLat, double originLon, double originAlt, int radarId, int deviceId)
{
    ensureInitialized();  // 确保 worker 和线程已初始化

    (void) QMetaObject::invokeMethod(_worker, "startForwarding", Qt::QueuedConnection,
                                     Q_ARG(QString, ip), Q_ARG(quint16, port), Q_ARG(double, frequencyHz),
                                     Q_ARG(double, originLat), Q_ARG(double, originLon), Q_ARG(double, originAlt),
                                     Q_ARG(int, radarId), Q_ARG(int, deviceId));
}

void DataForwardingSender::stopForwarding()
{
    (void) QMetaObject::invokeMethod(_worker, "stopForwarding", Qt::QueuedConnection);
}

void DataForwardingSender::sendData(const QByteArray &data)
{
    (void) QMetaObject::invokeMethod(_worker, "sendData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
}
