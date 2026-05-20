/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>
#include <QtPositioning/QGeoCoordinate>

Q_DECLARE_LOGGING_CATEGORY(DataForwardingSenderLog)

class Vehicle;

class DataForwardingWorker : public QObject
{
    Q_OBJECT

public:
    explicit DataForwardingWorker(QObject *parent = nullptr);
    virtual ~DataForwardingWorker();

public slots:
    void initialize();
    void startForwarding(const QString &ip, quint16 port, double frequencyHz,
                         double originLat, double originLon, double originAlt, int radarId, int deviceId);
    void stopForwarding();
    void sendData(const QByteArray &data);

signals:
    void forwardingStarted();
    void forwardingStopped();
    void errorOccurred(const QString &errorString);
    void dataSent(const QByteArray &data);
    void frequencyStatsUpdated(double actualFrequency, qint64 sendCount);
    void requestVehicleList();

private slots:
    void _onTimeout();
    void _updateVehicleList();
    void _onVehicleAdded(Vehicle* vehicle);
    void _onVehicleRemoved(Vehicle* vehicle);

private:
    QByteArray _buildPacket();
    struct ENUCoordinates {
        int32_t east;
        int32_t up;
        int32_t south;
    };
    ENUCoordinates _convertToENU(const QGeoCoordinate &vehicleCoord,
                                  double originLat, double originLon, double originAlt,
                                  double altitudeEllipsoid);

    QUdpSocket *_socket = nullptr;
    QHostAddress _targetAddress;
    quint16 _targetPort = 0;
    QTimer *_timer = nullptr;
    double _frequencyHz = 1.0;
    bool _isRunning = false;

    double _originLat = 0.0;
    double _originLon = 0.0;
    double _originAlt = 0.0;
    int _radarId = 0;
    int _deviceId = 0;

    QList<QPointer<Vehicle>> _vehicleList;
    mutable QMutex _vehicleListMutex;

    QElapsedTimer _elapsedTimer;
    qint64 _lastSendTime = 0;
    qint64 _actualSendCount = 0;
    qint64 _expectedSendCount = 0;
};

class DataForwardingSender : public QObject
{
    Q_OBJECT

public:
    explicit DataForwardingSender(QObject *parent = nullptr);
    virtual ~DataForwardingSender();

    static DataForwardingSender *instance();
    static void registerQmlTypes();

    Q_INVOKABLE void startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                     double originLat, double originLon, double originAlt, int radarId, int deviceId);
    Q_INVOKABLE void stopForwarding();
    Q_INVOKABLE void sendData(const QByteArray &data);

private:
    void ensureInitialized();

signals:
    void forwardingStarted();
    void forwardingStopped();
    void errorOccurred(const QString &errorString);
    void dataSent(const QByteArray &data);
    void frequencyStatsUpdated(double actualFrequency, qint64 sendCount);

private:
    DataForwardingWorker *_worker = nullptr;
    QThread *_workerThread = nullptr;
};
