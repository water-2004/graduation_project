# 基于边缘设备的心电异常检测系统

## 项目简介

本项目是一个毕业设计项目，目标是实现一个“基于边缘设备的心电异常检测系统”。

当前系统主要包含三部分：

- Python 数据处理与模型训练
- C++ 边缘推理服务
- Qt 桌面客户端

整体链路可以理解为：

`MIT-BIH 数据集 -> Python 训练模型 -> ONNX 模型 -> C++ 边缘推理服务 -> Qt 桌面端`

## 当前目录结构

```text
F:\graduation_project
├─ cpp/                C++ 推理与边缘服务
├─ widget/             Qt 桌面客户端
├─ scripts/            Python 数据处理与训练脚本
├─ shared/             共享组件（如日志系统）
├─ docs/               中文设计说明与开发文档
├─ MIT-BIH/            数据集（默认不纳入 Git）
├─ processed/          处理后的数据（默认不纳入 Git）
├─ artifacts/          模型产物（默认不纳入 Git）
└─ build/              构建目录（默认不纳入 Git）
```

## 系统组成

### 1. Python 训练部分

位于 `scripts/`：

- `01_profile_data.py`：数据概览与统计
- `02_prepare_mitbih.py`：MIT-BIH 数据准备与清洗
- `03_train_cnn.py`：训练 CNN 模型

### 2. C++ 边缘推理服务

位于 `cpp/`，当前核心目标：

- 加载 ONNX 模型
- 提供 TCP 推理服务
- 接收客户端请求
- 返回预测类别、置信度、告警等级和时延

当前协议：

- `PING`
- `PREDICT f1,f2,...,f187`
- `QUIT`

### 3. Qt 桌面客户端

位于 `widget/graduation_project_widget/`，当前功能：

- 连接边缘推理服务
- 发送 `PING`
- 输入 187 维特征并发送 `PREDICT`
- 展示推理结果与通信日志

当前客户端结构已经按分层方式整理为：

`MainWindow -> InferPage -> TcpMgr -> edge_infer_service`

## 日志系统

当前项目已经加入轻量统一日志系统：

- C++ 服务端和 Qt 客户端共用 `shared/logger.*`
- 支持控制台输出
- 支持日志文件落盘
- 支持关键日志分流

更多说明见：

- `docs/logging_system.md`

## 开发环境

当前建议环境：

- Windows 10/11
- Python / Anaconda
- Qt 6.8.3 MSVC2022 64bit
- Visual Studio 2022 Build Tools 或 Community
- CMake
- Boost.Asio
- ONNX Runtime

## 当前状态

当前已经完成：

- MIT-BIH 数据处理脚本初版
- CNN 模型训练流程初版
- C++ 边缘推理程序
- C++ 边缘推理服务
- Qt 客户端基础界面与网络分层
- 客户端 / 服务端统一日志系统

## 版本控制说明

为了避免仓库过大，以下内容默认不纳入 Git：

- 数据集目录 `MIT-BIH/`
- 构建目录 `build/`
- 处理后数据 `processed/`
- 模型产物 `artifacts/`
- 第三方大体积依赖 `third_party/`
- Word 原始材料目录 `doc/`

因此，GitHub 仓库主要保存：

- 源代码
- 文档
- 配置文件
- 项目结构

## 后续计划

后续准备继续完善：

1. 优化模型效果与训练指标
2. 完善边缘端服务能力
3. 扩展 Qt 客户端展示功能
4. 增加更完整的系统联调与演示流程
5. 补充毕业论文与答辩材料对应文档
