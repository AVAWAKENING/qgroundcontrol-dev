# QGroundControl 项目设计范式

## 1. Fact System 架构

### 1.1 Fact System 核心概念
QGroundControl 使用 Fact System 作为数据管理和绑定的核心机制：
- **Fact**: 单个数据项，包含值、元数据、单位转换等功能
- **FactGroup**: 一组相关 Fact 的集合，用于组织和管理相关数据
- **FactMetaData**: 定义 Fact 的元数据，包括类型、单位、范围、枚举值等

### 1.2 主要 FactGroup 类
- **VehicleFactGroup**: 车辆基础数据（姿态、速度、高度等）
- **VehicleGPSFactGroup**: GPS 相关数据（经纬度、卫星数、定位类型等）
- **VehicleBatteryFactGroup**: 电池相关数据
- **VehicleLocalPositionFactGroup**: 本地位置数据

### 1.3 Fact 数据定义
Fact 通过 JSON 文件定义元数据：
- 位置：`src/Vehicle/FactGroups/*.json`
- 示例：`GPSFact.json` 定义了 GPS 相关 Fact 的属性
- 包含：名称、类型、单位、枚举值、小数位数等

## 2. MAVLink 消息处理流程

### 2.1 消息分发机制
```
MAVLink消息 → Vehicle::mavlinkMessageReceived() 
           → 根据消息ID分发到对应的 FactGroup 
           → FactGroup::handleMessage() 
           → 更新对应的 Fact
           → 发出信号通知 UI 更新
```

### 2.2 主要消息处理函数
- **Vehicle.cc**: 处理车辆级别的消息（GPS_RAW_INT, GLOBAL_POSITION_INT等）
- **VehicleFactGroup.cc**: 处理姿态、高度等消息
- **VehicleGPSFactGroup.cc**: 处理 GPS 相关消息

### 2.3 消息优先级策略
系统使用标志位管理消息优先级：
- `_globalPositionIntMessageAvailable`: GLOBAL_POSITION_INT 优先于 GPS_RAW_INT
- `_altitudeMessageAvailable`: ALTITUDE 消息优先于其他高度数据源
- 当高优先级消息可用时，低优先级消息的数据将被忽略

## 3. 数据绑定和 UI 更新

### 3.1 QML 数据绑定
QML 通过属性绑定访问 Fact 数据：
```qml
// 访问 GPS 数据
activeVehicle.gps.lat.value        // 纬度
activeVehicle.gps.lon.value        // 经度
activeVehicle.gps.count.value      // 卫星数
activeVehicle.gps.lock.value       // 定位类型

// 访问车辆数据
activeVehicle.altitudeRelative.value   // 相对高度
activeVehicle.altitudeAMSL.value       // 海拔高度
activeVehicle.coordinate               // 位置坐标
```

### 3.2 信号机制
- Fact 值变化时自动发出信号
- QML 属性绑定自动更新 UI
- 使用 NOTIFY 信号实现响应式更新

## 4. 飞行界面数据来源详解

### 4.1 地图上 Vehicle 图标位置
**数据来源**: Vehicle.coordinate 属性
- **MAVLink 消息**:
  - 优先使用 `GLOBAL_POSITION_INT` 消息（如果可用）
  - 回退到 `GPS_RAW_INT` 消息（当 fix_type >= 3D_FIX 时）
  - 支持高延迟消息 `HIGH_LATENCY` 和 `HIGH_LATENCY2`
  
- **处理位置**:
  - [Vehicle.cc:735-754](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/Vehicle.cc#L735-L754): `_handleGpsRawInt()`
  - [Vehicle.cc:757-779](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/Vehicle.cc#L757-L779): `_handleGlobalPositionInt()`

- **UI 绑定**: [VehicleMapItem.qml:34](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/FlightMap/MapItems/VehicleMapItem.qml#L34)
  ```qml
  visible: coordinate.isValid
  // coordinate 属性来自 Vehicle 对象
  ```

### 4.2 飞行界面下方数据栏 GPS 数据

#### GPS 经纬度 (lat/lon)
**数据来源**: VehicleGPSFactGroup
- **Fact System**: `VehicleGPSFactGroup._latFact` 和 `_lonFact`
- **MAVLink 消息**: `GPS_RAW_INT`
  - lat: `gpsRawInt.lat * 1e-7`
  - lon: `gpsRawInt.lon * 1e-7`
- **处理位置**: [VehicleGPSFactGroup.cc:55-70](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/FactGroups/VehicleGPSFactGroup.cc#L55-L70)
- **UI 显示**: 通过 `TelemetryValuesBar.qml` 和 `HorizontalFactValueGrid.qml` 显示

#### GPS 定位类型 (lock)
**数据来源**: VehicleGPSFactGroup
- **Fact System**: `VehicleGPSFactGroup._lockFact`
- **MAVLink 消息**: `GPS_RAW_INT.fix_type`
- **枚举值**: None, 2D Lock, 3D Lock, 3D DGPS Lock, 3D RTK GPS Lock (float/fixed), Static
- **定义文件**: [GPSFact.json:43-48](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/FactGroups/GPSFact.json#L43-L48)

#### 相对高度 (altitudeRelative)
**数据来源**: VehicleFactGroup
- **Fact System**: `VehicleFactGroup._altitudeRelativeFact`
- **MAVLink 消息**:
  - 优先: `ALTITUDE.altitude_relative`
  - 回退: `GLOBAL_POSITION_INT.relative_alt / 1000.0`
- **处理位置**:
  - [VehicleFactGroup.cc:123-134](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/FactGroups/VehicleFactGroup.cc#L123-L134): `_handleAltitude()`
  - [Vehicle.cc:763](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/Vehicle.cc#L763): `_handleGlobalPositionInt()`

#### 海拔高度 (altitudeAMSL)
**数据来源**: VehicleFactGroup
- **Fact System**: `VehicleFactGroup._altitudeAMSLFact`
- **MAVLink 消息**:
  - 优先: `ALTITUDE.altitude_amsl`
  - 回退: `GLOBAL_POSITION_INT.alt / 1000.0`
  - 最后: `GPS_RAW_INT.alt / 1000.0`
- **处理位置**: 同相对高度

### 4.3 飞行界面上边栏 GPS 图标

#### 卫星数 (count)
**数据来源**: VehicleGPSFactGroup
- **Fact System**: `VehicleGPSFactGroup._countFact`
- **MAVLink 消息**: `GPS_RAW_INT.satellites_visible`
- **处理**: [VehicleGPSFactGroup.cc:63](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/FactGroups/VehicleGPSFactGroup.cc#L63)
  ```cpp
  count()->setRawValue((gpsRawInt.satellites_visible == 255) ? 0 : gpsRawInt.satellites_visible);
  ```
- **UI 显示**: [GPSIndicator.qml:72](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/QmlControls/GPSIndicator.qml#L72)
  ```qml
  text: _activeVehicle ? _activeVehicle.gps.count.valueString : ""
  ```

#### 定位类型 (lock)
**数据来源**: 同 4.2 中的定位类型
- **UI 显示**: [GPSIndicatorPage.qml:47](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/QmlControls/GPSIndicatorPage.qml#L47)
  ```qml
  labelText: activeVehicle ? activeVehicle.gps.lock.enumStringValue : na
  ```

## 5. 数据流架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      MAVLink 消息                            │
│  GPS_RAW_INT | GLOBAL_POSITION_INT | ALTITUDE | ...        │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Vehicle::mavlinkMessageReceived()              │
│              消息分发和路由中心                               │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│    Vehicle   │ │VehicleGPS    │ │VehicleFact   │
│              │ │FactGroup     │ │Group         │
│ coordinate   │ │              │ │              │
│ _handle...() │ │lat,lon,count │ │altitudeRel   │
│              │ │lock,hdop...  │ │altitudeAMSL  │
└──────┬───────┘ └──────┬───────┘ └──────┬───────┘
       │                │                │
       │                │                │
       ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────┐
│                    Fact System                               │
│  Fact 对象：值存储、元数据、信号发射                           │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              QML 属性绑定和信号响应                           │
│  vehicle.coordinate | vehicle.gps.lat | vehicle.altitude... │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│VehicleMapItem│ │TelemetryBar  │ │GPSIndicator  │
│  地图图标     │ │  数据栏      │ │  GPS图标     │
└──────────────┘ └──────────────┘ └──────────────┘
```

## 6. 关键设计模式

### 6.1 观察者模式
- Fact 作为被观察者，UI 组件作为观察者
- Fact 值变化时自动通知所有绑定的 UI 组件

### 6.2 策略模式
- 多个数据源（不同的 MAVLink 消息）提供相同类型的数据
- 通过标志位选择当前使用的数据源策略

### 6.3 分层架构
- **数据层**: MAVLink 消息接收和解析
- **业务层**: FactGroup 数据管理和处理
- **表示层**: QML UI 组件绑定和显示

### 6.4 依赖注入
- Vehicle 对象注入到各个 FactGroup
- FactGroup 通过 handleMessage() 接收消息

## 7. 文件组织结构

```
src/
├── Vehicle/
│   ├── Vehicle.h/cc              # 车辆核心类，消息分发中心
│   └── FactGroups/
│       ├── VehicleFactGroup.h/cc # 车辆基础数据
│       ├── VehicleGPSFactGroup.h/cc # GPS 数据
│       └── *.json                # Fact 元数据定义
├── FlightDisplay/
│   ├── FlyView.qml               # 飞行视图主界面
│   ├── TelemetryValuesBar.qml    # 数据栏
│   └── MultiVehicleList.qml      # 多车辆列表
├── FlightMap/
│   └── MapItems/
│       └── VehicleMapItem.qml    # 地图车辆图标
└── QmlControls/
    ├── GPSIndicator.qml          # GPS 指示器
    └── GPSIndicatorPage.qml      # GPS 详情页
```

## 8. 最佳实践

### 8.1 数据访问
- 优先使用 Fact System 提供的数据，而不是直接解析 MAVLink 消息
- 通过 Vehicle 对象的属性访问数据，保持一致性

### 8.2 UI 绑定
- 使用 QML 属性绑定，避免手动更新
- 利用 Fact 的 valueString 属性获取格式化的显示值

### 8.3 扩展新数据
1. 在对应的 FactGroup 中添加 Fact 成员
2. 在 JSON 文件中定义 Fact 元数据
3. 实现 handleMessage() 中的消息处理
4. 在 QML 中通过属性绑定使用

### 8.4 调试技巧
- 使用 `qCDebug(VehicleLog)` 输出调试信息
- 检查 Fact 的 raw Value 和 cookedValue
- 验证 MAVLink 消息是否正确接收和解析
