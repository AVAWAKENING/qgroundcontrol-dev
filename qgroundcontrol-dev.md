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

## 9. 通信链路架构

### 9.1 核心对象定义

#### LinkInterface
**定义**: [LinkInterface.h:21](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Comms/LinkInterface.h#L21)

**作用**: 代表一个物理通信链路（串口、UDP、TCP、蓝牙等）

**关键属性**:
- `_config: SharedLinkConfigurationPtr` - 链路配置
- `_mavlinkChannel: uint8_t` - MAVLink 通道号
- `_vehicleReferenceCount: int` - 引用该链路的车辆数量

**关键方法**:
- `writeBytesThreadSafe()` - 线程安全地发送数据
- `isConnected()` - 检查连接状态
- `mavlinkChannel()` - 获取 MAVLink 通道

**子类实现**:
- `SerialLink` - 串口链路
- `UDPLink` - UDP 链路
- `TCPLink` - TCP 链路
- `MockLink` - 模拟链路（测试用）

#### LinkConfiguration
**定义**: [LinkConfiguration.h:18](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Comms/LinkConfiguration.h#L18)

**作用**: 存储链路的配置信息（IP 地址、端口、串口名称等）

**关键属性**:
- `_name: QString` - 链路名称（如 "Radio Link"）
- `_link: LinkInterface*` - 关联的链路对象
- `_dynamic: bool` - 是否为动态链路
- `_autoConnect: bool` - 是否自动连接

#### LinkManager
**定义**: [LinkManager.h:40](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Comms/LinkManager.h#L40)

**作用**: 单例模式，创建和管理所有 LinkInterface 对象

**关键属性**:
- `_rgLinks: QList<SharedLinkInterfacePtr>` - 所有链路对象
- `_rgLinkConfigs: QList<SharedLinkConfigurationPtr>` - 所有链路配置

**关键方法**:
- `createConnectedLink()` - 创建并连接链路
- `sharedLinkInterfacePointerForLink()` - 获取链路的 shared_ptr

#### Vehicle
**定义**: [Vehicle.h:90](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/Vehicle.h#L90)

**作用**: 代表一个连接的飞行器，包含飞行器的所有状态和信息

**关键属性**:
- `_id: int` - 飞行器 ID
- `_vehicleLinkManager: VehicleLinkManager*` - 链路管理器
- `_firmwarePlugin: FirmwarePlugin*` - 固件插件

#### VehicleLinkManager
**定义**: [VehicleLinkManager.h:25](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/VehicleLinkManager.h#L25)

**作用**: 每个 Vehicle 有一个，管理该车辆使用的所有链路

**关键属性**:
- `_rgLinkInfo: QList<LinkInfo_t>` - 该车辆使用的所有链路
- `_primaryLink: WeakLinkInterfacePtr` - 主链路

**关键方法**:
- `primaryLink()` - 获取主链路
- `mavlinkMessageReceived()` - 处理接收到的 MAVLink 消息

#### MultiVehicleManager
**定义**: [MultiVehicleManager.h:26](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/MultiVehicleManager.h#L26)

**作用**: 单例模式，管理所有连接的 Vehicle 对象

**关键属性**:
- `_vehicles: QmlObjectListModel*` - 所有车辆列表
- `_activeVehicle: Vehicle*` - 当前活动的车辆

### 9.2 对象关系架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         QGroundControl                           │
└─────────────────────────────────────────────────────────────────┘
                                │
                ┌───────────────┴───────────────┐
                │                               │
        ┌───────▼────────┐              ┌──────▼──────┐
        │  LinkManager   │              │MultiVehicle │
        │   (单例)        │              │  Manager    │
        │                │              │  (单例)      │
        └───────┬────────┘              └──────┬──────┘
                │                               │
                │ 管理所有链路                    │ 管理所有车辆
                │                               │
        ┌───────┴────────┐              ┌──────┴──────┐
        │                │              │             │
   ┌────▼────┐      ┌────▼────┐    ┌───▼────┐   ┌───▼────┐
   │UDPLink  │      │SerialLink│    │Vehicle │   │Vehicle │
   │(链路1)  │      │(链路2)   │    │  (1)   │   │  (2)   │
   └────┬────┘      └────┬────┘    └───┬────┘   └───┬────┘
        │                │              │             │
        │                │              │             │
   ┌────▼────┐      ┌────▼────┐    ┌───▼────┐   ┌───▼────┐
   │UDPConfig│      │SerialConf│    │Vehicle │   │Vehicle │
   │(配置1)  │      │(配置2)   │    │LinkMgr │   │LinkMgr │
   │"Radio"  │      │"USB"     │    └───┬────┘   └───┬────┘
   └─────────┘      └──────────┘        │             │
                                          │             │
                                    ┌─────┴─────┐  ┌───┴─────┐
                                    │ primaryLink│  │primaryLn│
                                    │  (weak_ptr)│  │(weak_ptr)│
                                    └─────┬─────┘  └───┬─────┘
                                          │             │
                                    ┌─────┴─────────────┴─────┐
                                    │                         │
                                    │    共享同一个 LinkInterface │
                                    │                         │
                                    └─────────────────────────┘
                                              │
                                        ┌─────▼─────┐
                                        │ UDPLink   │
                                        │(0x7f1234) │
                                        │"Radio"    │
                                        └───────────┘
```

### 9.3 链路共享机制（星型拓扑）

在星型拓扑网络中，多个 Vehicle 共享同一个物理链路：

```
物理链路：一个 UDP 链路（如 192.168.1.100:14550）
         ↓
LinkManager 创建一个 LinkInterface 对象
         ↓
    ┌────┴────┬─────────┐
    │         │         │
Vehicle1  Vehicle2  Vehicle3
    │         │         │
    └────┬────┴─────────┘
         │
所有 Vehicle 的 primaryLink 都指向同一个 LinkInterface 对象
```

**代码证据**: [VehicleLinkManager.cc:191](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/Vehicle/VehicleLinkManager.cc#L191)
```cpp
SharedLinkInterfacePtr sharedLink = LinkManager::instance()->sharedLinkInterfacePointerForLink(link);
```

### 9.4 数据流向

**接收数据**:
```
物理链路 → LinkInterface → MAVLinkProtocol → Vehicle → VehicleLinkManager
```

**发送数据**:
```
Vehicle → VehicleLinkManager → primaryLink → LinkInterface → 物理链路
```

### 9.5 链路引用计数

`LinkInterface` 有一个 `_vehicleReferenceCount`，记录有多少个 Vehicle 在使用该链路：

```cpp
void addVehicleReference() { ++_vehicleReferenceCount; }
void removeVehicleReference();
```

### 9.6 设计要点

1. **链路共享**: 多个 Vehicle 可以共享同一个 LinkInterface 对象（星型拓扑）
2. **引用计数**: LinkInterface 使用引用计数管理生命周期
3. **主链路概念**: 每个 Vehicle 有一个 primaryLink，用于主要通信
4. **线程安全**: 使用 `writeBytesThreadSafe()` 进行线程安全的数据发送

### 9.7 常见问题

#### RTCM 数据重复发送问题
在星型拓扑网络中，如果多个 Vehicle 共享同一个 LinkInterface，RTCM 数据应该只发送一次。

**解决方案**:
- 使用 `QSet<LinkInterface*>` 去重
- 或使用链路名称去重（如果 LinkInterface 对象不同）

**参考**: [RTCMMavlink.cc](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/GPS/RTCMMavlink.cc)

## 10. UI 组件开发范式

### 10.1 工具栏按钮添加
在 FlyViewToolBar 中添加按钮的标准流程：
- **位置**: [FlyViewToolBar.qml](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/QmlControls/FlyViewToolBar.qml)
- **结构**: 工具栏采用水平布局，包含三个主要区域：
  1. `viewButtonRow`: 左侧按钮组（Logo、状态指示器、断开按钮）
  2. `toolsFlickable`: 中间可滚动的指示器区域
  3. 右侧区域：品牌Logo或其他按钮

- **添加按钮步骤**:
  1. 在工具栏右侧添加 QGCButton 组件
  2. 使用 anchors 定位按钮位置
  3. 在 onClicked 事件中创建弹窗组件
  4. 调整 toolsFlickable 的 anchors.right 以避免重叠

- **示例代码**:
  ```qml
  QGCButton {
      id:                     dataForwardingButton
      anchors.rightMargin:    ScreenTools.defaultFontPixelWidth / 2
      anchors.right:          parent.right
      anchors.verticalCenter: parent.verticalCenter
      text:                   qsTr("数据转发")
      onClicked:              dialogComponent.createObject(mainWindow).open()
  }
  ```

### 10.2 悬浮窗口/弹窗组件开发
创建自定义悬浮窗口的标准方法：

#### 方法一：基于 Popup 组件
- **适用场景**: 非模态、可点击外部关闭的悬浮窗口
- **实现方式**:
  ```qml
  Popup {
      id:                 popup
      parent:             Overlay.overlay
      modal:              false
      focus:              true
      closePolicy:        Popup.CloseOnEscape | Popup.CloseOnPressOutside
      anchors.centerIn:   parent
      
      background: Rectangle { color: "transparent" }
      
      contentItem: CustomComponent {
          oncloseClicked: popup.close()
      }
  }
  ```

#### 方法二：基于 QGCPopupDialog
- **适用场景**: 模态对话框、需要确认/取消按钮
- **特点**: 
  - 自动管理生命周期（destroyOnClose）
  - 支持标准按钮（Ok, Cancel, Save等）
  - 自动居中显示
- **参考**: [QGCPopupDialog.qml](file:///home/hz-rd-01-sfq01/01Projects/001QGC/qgroundcontrol-dev/src/QmlControls/QGCPopupDialog.qml)

### 10.3 自定义UI组件开发
开发自定义UI组件的最佳实践：

#### 组件结构
- **根元素**: 通常使用 Rectangle 作为容器
- **布局**: 使用 ColumnLayout、RowLayout、GridLayout 组织内容
- **样式**: 使用 QGCPalette 获取主题颜色
- **尺寸**: 使用 ScreenTools 获取标准尺寸

#### 输入控件
- **文本输入**: QGCTextField
  ```qml
  QGCTextField {
      id:                 inputField
      Layout.fillWidth:   true
      placeholderText:    qsTr("提示文本")
      text:               "默认值"
  }
  ```

- **开关**: QGCSwitch
  ```qml
  QGCSwitch {
      id:         toggleSwitch
      checked:    false
      onCheckedChanged: {
          // 处理状态变化
      }
  }
  ```

#### 信号机制
- **定义信号**: `signal closeClicked()`
- **发射信号**: `onClicked: root.closeClicked()`
- **连接信号**: `oncloseClicked: popup.close()`

### 10.4 QML 模块注册
新创建的 QML 组件需要注册到模块中：

- **位置**: `src/QmlControls/CMakeLists.txt`
- **步骤**:
  1. 在 `qt_add_qml_module` 的 `QML_FILES` 列表中添加文件名
  2. 按字母顺序插入到合适位置
  3. 重新构建项目

- **示例**:
  ```cmake
  qt_add_qml_module(QGroundControlControlsModule
      URI QGroundControl.Controls
      VERSION 1.0
      RESOURCE_PREFIX /qml
      QML_FILES
          ...
          DataForwardingSettings.qml
          ...
  )
  ```

### 10.5 样式和主题
QGroundControl 使用统一的样式系统：

#### 颜色系统
- **QGCPalette**: 提供主题相关的颜色
  ```qml
  QGCPalette { id: qgcPal }
  
  color: qgcPal.window           // 窗口背景色
  color: qgcPal.windowShade      // 窗口阴影色
  color: qgcPal.text             // 文本颜色
  color: qgcPal.button           // 按钮颜色
  ```

#### 尺寸规范
- **ScreenTools**: 提供标准尺寸
  ```qml
  ScreenTools.defaultFontPixelWidth      // 默认字体宽度
  ScreenTools.defaultFontPixelHeight     // 默认字体高度
  ScreenTools.mediumFontPointSize        // 中号字体
  ScreenTools.smallFontPointSize         // 小号字体
  ```

#### 边距和间距
- **标准边距**: `ScreenTools.defaultFontPixelHeight * 0.5`
- **控件间距**: `ScreenTools.defaultFontPixelWidth`
- **圆角半径**: `ScreenTools.defaultFontPixelWidth / 2`

### 10.6 布局最佳实践
使用 Qt Quick Layouts 进行响应式布局：

#### GridLayout
适用于表单类布局：
```qml
GridLayout {
    columns:        2
    rowSpacing:     ScreenTools.defaultFontPixelHeight * 0.25
    columnSpacing:  ScreenTools.defaultFontPixelWidth
    
    QGCLabel { text: qsTr("标签:") }
    QGCTextField {
        Layout.fillWidth: true
        placeholderText: qsTr("输入")
    }
}
```

#### ColumnLayout
适用于垂直排列的组件：
```qml
ColumnLayout {
    spacing: ScreenTools.defaultFontPixelHeight * 0.5
    
    RowLayout {
        Layout.fillWidth: true
        // 标题栏
    }
    
    GridLayout {
        // 表单内容
    }
}
```

### 10.7 国际化
所有用户可见的字符串应使用 qsTr() 包裹：
```qml
text: qsTr("数据转发")
placeholderText: qsTr("例如: 192.168.1.100")
```

### 10.8 数据持久化
使用 QtCore.Settings 实现简单的数据持久化（Qt 6.5+）：

#### Settings 组件
- **导入**: `import QtCore`（Qt 6.5+ 推荐方式）
- **旧版导入**: `import Qt.labs.settings`（已弃用，不推荐使用）
- **特点**: 自动保存到配置文件，应用重启后自动加载
- **存储位置**: 
  - Linux: `~/.config/QGroundControl/QGroundControl Daily.ini`
  - Windows: 注册表或配置文件
  - macOS: `~/Library/Preferences/com.qgroundcontrol.QGroundControl.plist`

#### 实现示例
```qml
import QtCore

Rectangle {
    Settings {
        id: settings
        property string ipAddress:      "127.0.0.1"
        property string portNumber:     "14550"
        property bool   forwardingEnabled: false
    }
    
    QGCTextField {
        text:           settings.ipAddress
        onTextChanged:  settings.ipAddress = text
    }
}
```

#### 注意事项
- Settings 属性会在值改变时自动保存
- 首次运行使用默认值
- 支持基本类型：string, bool, int, real, var 等
- 不支持复杂对象，需要序列化
- **重要**: Qt 6.5+ 应使用 `import QtCore`，而不是 `import Qt.labs.settings`

### 10.9 可拉伸窗口实现
实现窗口大小可调整的功能：

#### 拖拽区域
```qml
MouseArea {
    id: resizeMouseArea
    anchors.right:    parent.right
    anchors.bottom:   parent.bottom
    width:            ScreenTools.defaultFontPixelWidth * 2
    height:           ScreenTools.defaultFontPixelHeight * 2
    cursorShape:      Qt.SizeFDiagCursor  // 对角线调整光标
    
    property real lastX: 0
    property real lastY: 0
    
    onPressed: {
        lastX = mouse.x
        lastY = mouse.y
    }
    
    onMouseXChanged: {
        var dx = mouse.x - lastX
        var newWidth = root.width + dx
        if (newWidth >= minWidth) {
            root.width = newWidth
        }
    }
    
    onMouseYChanged: {
        var dy = mouse.y - lastY
        var newHeight = root.height + dy
        if (newHeight >= minHeight) {
            root.height = newHeight
        }
    }
}
```

#### 视觉指示器
使用 Canvas 绘制调整大小的图标：
```qml
Canvas {
    anchors.fill: parent
    onPaint: {
        var ctx = getContext("2d")
        ctx.strokeStyle = qgcPal.text
        ctx.lineWidth = 2
        
        // 绘制两条斜线
        ctx.beginPath()
        ctx.moveTo(width * 0.7, height * 0.3)
        ctx.lineTo(width * 0.3, height * 0.7)
        ctx.stroke()
        
        ctx.beginPath()
        ctx.moveTo(width * 0.9, height * 0.3)
        ctx.lineTo(width * 0.3, height * 0.9)
        ctx.stroke()
    }
}
```

#### 滚动支持
使用 QGCFlickable 包裹内容，支持窗口缩小后滚动查看：
```qml
QGCFlickable {
    anchors.fill: parent
    contentWidth: mainLayout.width
    contentHeight: mainLayout.height
    flickableDirection: Flickable.VerticalFlick
    clip: true
    
    ColumnLayout {
        id: mainLayout
        width: parent.width - margins * 2
        // 内容...
    }
}
```
