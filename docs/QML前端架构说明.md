# QML 前端架构说明

## 1. 改造目标

本次将原有 `QWidget` 客户端主界面切换为 `QML + C++ Backend` 架构，目标是：

- 将登录页与主界面彻底拆开
- 建立统一的设计系统，避免页面风格割裂
- 采用“左侧导航栏 + 顶部状态栏 + 主内容区”的整体壳结构
- 让界面层专注展示，业务逻辑继续复用已有 C++ 网络与服务代码
- 为后续继续扩展 Qt 前端、病历功能、消息通知、更多图表预留空间

## 2. 当前前端技术路线

### 2.1 架构分层

当前 Qt 客户端已经采用如下结构：

- `UI/`
  - 纯 QML 页面与组件
- `ViewModels/`
  - 暴露给 QML 的视图模型
- `Models/`
  - 列表模型、波形流模型、时间轴模型
- `Backend/`
  - 对原有 `BusinessClient`、`TcpMgr` 的包装
- 旧 `business_client.cpp/.h`
  - 业务服务连接与协议收发
- 旧 `tcpmgr.cpp/.h`
  - 边缘推理服务连接与协议收发

也就是说，现在不是把业务逻辑写进 QML，而是：

1. QML 负责展示和交互
2. `AppViewModel / MonitorViewModel / DashboardViewModel` 负责页面数据组织
3. `AuthService / NetworkService` 继续复用原来已经写好的 C++ 网络逻辑

### 2.2 页面结构

当前 QML 页面包括：

- `UI/Pages/LoginPage.qml`
  - 独立登录页
- `UI/Pages/DashboardPage.qml`
  - 系统概览页
- `UI/Pages/PatientPage.qml`
  - 病人管理页，采用“列表 + 右侧抽屉”模式
- `UI/Pages/MonitorPage.qml`
  - 实时监测页，包含深色波形区、推理结果区、历史片段区
- `UI/Pages/AlertCenterPage.qml`
  - 报警中心页，采用左右分栏 triage 布局
- `UI/Pages/AboutPage.qml`
  - 系统说明页

### 2.3 公共组件

公共组件位于 `UI/Components/`：

- `Sidebar.qml`
  - 左侧导航栏
- `TopBar.qml`
  - 顶部状态栏
- `Toast.qml`
  - 非阻塞提示条
- `MetricCard.qml`
  - 指标卡片
- `SectionCard.qml`
  - 通用卡片容器
- `StatusPill.qml`
  - 状态胶囊
- `WaveCanvas.qml`
  - 心电波形画布
- `LineChart.qml`
  - 趋势折线图
- `DonutChart.qml`
  - 环形分布图
- `RadarChart.qml`
  - 雷达图

## 3. C++ 桥接层说明

### 3.1 AppViewModel

文件：`widget/graduation_project_widget/ViewModels/AppViewModel.*`

职责：

- 管理登录状态
- 管理当前页面导航
- 汇总顶栏所需的用户信息、业务服务状态、通知数量
- 调度病人管理、报警管理、Dashboard 和 Monitor 等子模块

### 3.2 DashboardViewModel

文件：`widget/graduation_project_widget/ViewModels/DashboardViewModel.*`

职责：

- 统计病人总数、监测记录数、报警数
- 从 `model_metrics.json` 读取模型指标
- 组装报警趋势、类别分布、雷达图数据
- 组装最近动态时间轴

### 3.3 MonitorViewModel

文件：`widget/graduation_project_widget/ViewModels/MonitorViewModel.*`

职责：

- 管理边缘端连接参数和连接状态
- 处理样本加载、样本播放、手动推理
- 保存当前推理结果、置信度、延时、告警级别
- 推送波形给 `EcgDataStream`
- 将监测结果转成监测记录发送给业务服务

### 3.4 Backend 层

- `AuthService`
  - 包装 `BusinessClient`
  - 负责登录、病人管理、报警管理、监测记录写回
- `NetworkService`
  - 包装 `TcpMgr`
  - 负责边缘推理服务连接、PING、预测、样本播放
- `AlertManager`
  - 包装 `AlertModel`
  - 负责当前报警选中项和统计

## 4. 构建系统

当前 `widget/graduation_project_widget/CMakeLists.txt` 已切换到：

- `Qt6::Gui`
- `Qt6::Quick`
- `Qt6::QuickControls2`
- `Qt6::Qml`
- `Qt6::Network`

主入口已经从旧的 `MainWindow(QWidget)` 改为：

- `main.cpp`
- `QGuiApplication + QQmlApplicationEngine`

## 5. 当前运行效果

已经完成并验证：

- QML 工程可以成功 `cmake configure`
- QML 工程可以成功 `cmake --build`
- 可执行程序可以启动，说明主入口与 QML 资源链路已经打通

## 6. 在本机上的构建方式

### 6.1 配置

```powershell
cmake -S F:\graduation_project\widget\graduation_project_widget `
  -B F:\graduation_project\build\widget_qml `
  -G Ninja `
  -DCMAKE_PREFIX_PATH=F:/Qt/6.8.1/mingw_64 `
  -DCMAKE_MAKE_PROGRAM=F:/Qt/Tools/Ninja/ninja.exe `
  -DCMAKE_C_COMPILER=F:/Qt/Tools/mingw1310_64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=F:/Qt/Tools/mingw1310_64/bin/g++.exe
```

### 6.2 编译

```powershell
cmake --build F:\graduation_project\build\widget_qml -j4
```

### 6.3 运行

```powershell
F:\graduation_project\build\widget_qml\graduation_project_widget.exe
```

## 7. 当前仍可继续完善的点

虽然这次已经完成了前端从 `QWidget` 到 `QML` 的主切换，但仍然还有可继续迭代的部分：

- 为按钮、输入框、下拉框进一步统一自定义控件风格
- 为实时监测页增加更完整的播放控制组件
- 为报警页增加病历摘要、备注、联动消息接口
- 为病人管理页增加删除确认弹窗
- 为 Dashboard 增加更细的图表动画与数字滚动效果
- 增加登录失败、网络异常、边缘断连等专门状态页

## 8. 与毕设整体的关系

这次前端改造后，你的系统结构已经更清晰：

- Python：负责数据处理、模型训练、导出 ONNX
- C++ 边缘端：负责香橙派推理服务
- C++ 业务服务：负责账号、病人、记录、报警持久化
- Qt/QML 客户端：负责医生/管理端可视化与交互

这对毕业设计答辩是加分项，因为它已经不只是“一个能跑的界面”，而是一个分层明确、前后端职责清楚的完整系统。
