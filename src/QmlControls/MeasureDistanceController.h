/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QtPositioning/QGeoCoordinate>

class MeasureDistanceController : public QObject
{
    Q_OBJECT

public:
    explicit MeasureDistanceController(QObject* parent = nullptr);
    ~MeasureDistanceController();

    Q_PROPERTY(bool     enabled         READ enabled         WRITE setEnabled         NOTIFY enabledChanged)
    Q_PROPERTY(int      clickCount      READ clickCount                               NOTIFY clickCountChanged)
    Q_PROPERTY(QGeoCoordinate startPoint  READ startPoint                               NOTIFY startPointChanged)
    Q_PROPERTY(QGeoCoordinate endPoint    READ endPoint                                 NOTIFY endPointChanged)
    Q_PROPERTY(double   distance        READ distance                                 NOTIFY distanceChanged)
    Q_PROPERTY(bool     hasValidStart   READ hasValidStart                            NOTIFY startPointChanged)
    Q_PROPERTY(bool     hasValidEnd     READ hasValidEnd                              NOTIFY endPointChanged)

    bool enabled() const { return _enabled; }
    int clickCount() const { return _clickCount; }
    QGeoCoordinate startPoint() const { return _startPoint; }
    QGeoCoordinate endPoint() const { return _endPoint; }
    double distance() const { return _distance; }
    bool hasValidStart() const { return _startPoint.isValid(); }
    bool hasValidEnd() const { return _endPoint.isValid(); }

    void setEnabled(bool enabled);

    Q_INVOKABLE void clearPoints();
    Q_INVOKABLE void handleMapClick(const QGeoCoordinate& coordinate);
    Q_INVOKABLE QString distanceString() const;

signals:
    void enabledChanged(bool enabled);
    void clickCountChanged(int clickCount);
    void startPointChanged(QGeoCoordinate startPoint);
    void endPointChanged(QGeoCoordinate endPoint);
    void distanceChanged(double distance);

private:
    void calculateDistance();

    bool            _enabled = false;
    int             _clickCount = 0;
    QGeoCoordinate  _startPoint;
    QGeoCoordinate  _endPoint;
    double          _distance = 0.0;
};