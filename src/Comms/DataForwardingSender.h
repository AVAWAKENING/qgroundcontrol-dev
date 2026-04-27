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
#include <QtCore/QObject>
#include <QtCore/QTimer>
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
    void startForwarding(const QString &ip, quint16 port, double frequencyHz,
                         double originLat, double originLon, double originAltEllipsoid, int radarId, int deviceId);
    void stopForwarding();
    void sendData(const QByteArray &data);
    void updateFrequency(double frequencyHz);

signals:
    void forwardingStarted();
    void forwardingStopped();
    void errorOccurred(const QString &errorString);
    void dataSent(const QByteArray &data);
    void requestVehicleList();

private slots:
    void _onTimeout();
    void _updateVehicleList();
    void _onVehicleCoordinateChanged();

private:
    QByteArray _buildPacket();
    int32_t _convertToEastUpSouth(const QGeoCoordinate &vehicleCoord,
                                  double originLat, double originLon, double originAltEllipsoid,
                                  char axis); // 'x'=East, 'y'=Up, 'z'=South

    QUdpSocket *_socket = nullptr;
    QHostAddress _targetAddress;
    quint16 _targetPort = 0;
    QTimer *_timer = nullptr;
    double _frequencyHz = 1.0;
    bool _isRunning = false;

    double _originLat = 0.0;
    double _originLon = 0.0;
    double _originAltEllipsoid = 0.0;
    int _radarId = 0;
    int _deviceId = 0;

    QList<Vehicle*> _vehicleList;
};

class DataForwardingSender : public QObject
{
    Q_OBJECT

public:
    explicit DataForwardingSender(QObject *parent = nullptr);
    virtual ~DataForwardingSender();

    Q_INVOKABLE void startForwarding(const QString &ip, quint16 port, double frequencyHz,
                                     double originLat, double originLon, double originAltEllipsoid, int radarId, int deviceId);
    Q_INVOKABLE void stopForwarding();
    Q_INVOKABLE void sendData(const QByteArray &data);
    Q_INVOKABLE void updateFrequency(double frequencyHz);

signals:
    void forwardingStarted();
    void forwardingStopped();
    void errorOccurred(const QString &errorString);
    void dataSent(const QByteArray &data);

private:
    DataForwardingWorker *_worker = nullptr;
    QThread *_workerThread = nullptr;
};
