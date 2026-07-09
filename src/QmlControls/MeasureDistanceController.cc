/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MeasureDistanceController.h"
#include <QGeoCoordinate>
#include <QDebug>

MeasureDistanceController::MeasureDistanceController(QObject* parent)
    : QObject(parent)
{
}

MeasureDistanceController::~MeasureDistanceController()
{
}

void MeasureDistanceController::setEnabled(bool enabled)
{
    if (_enabled != enabled) {
        _enabled = enabled;
        if (!_enabled) {
            // 当禁用时,清除所有标记点和计数
            clearPoints();
        }
        emit enabledChanged(enabled);
    }
}

void MeasureDistanceController::clearPoints()
{
    _startPoint = QGeoCoordinate();
    _endPoint = QGeoCoordinate();
    _clickCount = 0;
    _distance = 0.0;
    
    emit startPointChanged(_startPoint);
    emit endPointChanged(_endPoint);
    emit clickCountChanged(_clickCount);
    emit distanceChanged(_distance);
}

void MeasureDistanceController::handleMapClick(const QGeoCoordinate& coordinate)
{
    if (!_enabled) {
        return;
    }

    if (!coordinate.isValid()) {
        qWarning() << "Invalid coordinate received in handleMapClick";
        return;
    }

    _clickCount++;
    emit clickCountChanged(_clickCount);

    // 奇数次点击:清除之前的标记,设置起点
    if (_clickCount % 2 == 1) {
        _startPoint = coordinate;
        _endPoint = QGeoCoordinate();  // 清除终点
        _distance = 0.0;
        
        emit startPointChanged(_startPoint);
        emit endPointChanged(_endPoint);
        emit distanceChanged(_distance);
        
        qDebug() << "起点已设置:" << coordinate.toString();
    } else {
        // 偶数次点击:设置终点
        _endPoint = coordinate;
        emit endPointChanged(_endPoint);
        
        qDebug() << "终点已设置:" << coordinate.toString();
        
        // 计算距离
        calculateDistance();
    }
}

void MeasureDistanceController::calculateDistance()
{
    if (_startPoint.isValid() && _endPoint.isValid()) {
        _distance = _startPoint.distanceTo(_endPoint);
        emit distanceChanged(_distance);
        qDebug() << "计算距离:" << _distance << "米";
    } else {
        _distance = 0.0;
        emit distanceChanged(_distance);
    }
}

QString MeasureDistanceController::distanceString() const
{
    if (_distance < 1000.0) {
        return QString::number(_distance, 'f', 2) + " m";
    } else {
        return QString::number(_distance / 1000.0, 'f', 2) + " km";
    }
}