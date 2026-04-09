# 基于边缘计算的心电异常检测系统

## 项目简介

本项目是毕业设计项目，目标是实现一套面向边缘设备部署的心电异常检测系统。系统以 MIT-BIH 心电数据集为基础，使用 Python 完成数据处理、模型训练与 ONNX 导出，使用 C++ 实现边缘推理服务与业务服务，使用 Qt/QML 实现桌面监测客户端，最终形成“模型训练 + 边缘推理 + 业务管理 + 可视化监测”的完整闭环。

当前系统主要面向课程设计 / 毕业设计答辩场景，已经完成从样本加载、模型推理、告警生成到前端展示的主流程联调。当前采集端采用模拟数据链路，后续可以继续扩展真实传感器接入。

## 项目特点

- 以参考文献《Reliable ECG Anomaly Detection on Edge Devices for Internet of Medical Things Applications》的方法路线为主线，复现并优化 `STFT + 2D-CNN` 模型。
- 支持将训练得到的模型导出为 ONNX，并部署到香橙派 3（Orange Pi 3，2GB）等边缘设备上运行。
- 使用 C++ 构建双服务端架构：`edge_infer_service` 负责边缘推理，`business_service` 负责登录、病人、监测记录、报警等业务。
- 使用 Qt/QML 构建桌面客户端，当前界面流程为“登录 -> 主界面 -> 仪表盘 / 病人管理 / 实时监测 / 报警中心 / 关于页面”。
- 已实现严重报警弹窗、提示音、监测历史、网络超时与自动重连、密码加密存储、SQLite 持久化、日志记录等功能。
- 旧版 QWidget 客户端代码已归档，当前主开发版本为 QML 前端。

## 系统架构

```text
MIT-BIH 数据集 / 模拟采样数据
  -> Python 数据清洗与模型训练
  -> ONNX 模型导出
  -> C++ 边缘推理服务 edge_infer_service
  -> C++ 业务服务 business_service
  -> Qt/QML 客户端 graduation_project_widget
```

### 1. 训练与模型侧

- 数据分析与清洗：`scripts/01_profile_data.py`、`scripts/02_prepare_mitbih.py`
- 基线模型训练：`scripts/03_train_cnn.py`、`scripts/05_train_random_forest.py`、`scripts/06_train_xgboost.py`
- 论文方法复现：`scripts/09_train_paper_reference.py`
- 论文方法优化：`scripts/11_train_paper_optimized.py`
- 多模型综合对比：`scripts/12_compare_all_models.py`

### 2. 边缘推理侧

`edge_infer_service` 负责：

- 加载 ONNX 模型
- 接收客户端发送的心电样本
- 完成前处理、模型推理与结果封装
- 返回预测类别、置信度、报警等级、推理耗时等结果

当前支持两类模型输入：

1. 一维心拍输入 `[N,1,187]`
2. 论文参考模型输入 `[N,1,17,11]`

当加载论文参考模型或其优化模型时，边缘端会自动完成：

1. 带通滤波
2. 归一化
3. STFT 变换
4. 构造 `1 x 17 x 11` 输入张量
5. 调用 ONNX Runtime 推理

### 3. 业务服务侧

`business_service` 负责：

- 登录认证
- 用户密码校验与修改
- 病人信息管理
- 监测记录保存
- 报警记录生成、查询、确认
- SQLite 数据持久化
- 与 Qt 客户端的业务协议交互

### 4. Qt/QML 客户端侧

当前客户端位于 `widget/graduation_project_widget/`，采用 QML + C++ Backend 的分层结构：

- `Backend/`：网络、认证、报警管理
- `Models/`：病人、报警、历史、时间线、波形数据模型
- `ViewModels/`：页面状态与业务协调
- `UI/Components/`：通用组件
- `UI/Pages/`：登录、仪表盘、病人管理、监测、报警中心、关于页面

旧版 QWidget 客户端已归档到：`widget/graduation_project_widget_legacy/`

## 当前模型选择与实验结论

本项目前期完成了多模型对比，后期将“参考文献算法复现”作为主线推进。当前仓库默认展示和部署的主模型为：

- `OptimizedReferencePaperSTFT2DCNN`
- 即：基于参考文献路线的优化版 `STFT + 2D-CNN`

当前模型指标来自 `widget/graduation_project_widget/data/model_metrics.json`：

| 模型 | Accuracy | Macro-F1 | Macro-Recall | 说明 |
| --- | ---: | ---: | ---: | --- |
| 优化后参考论文 STFT+2D-CNN | 97.41% | 87.74% | 91.40% | 当前系统默认展示 / 部署模型 |
| Random Forest | 97.91% | 89.93% | 89.42% | 对比实验综合指标最优 |
| XGBoost | 97.12% | 86.57% | 90.28% | 对比实验次优 |
| 论文风格 STFT+2D-CNN（早期版） | 92.86% | 74.86% | 91.29% | 初版复现结果 |
| 1D-CNN | 90.49% | 71.42% | 88.28% | 早期工程基线 |

### 为什么当前系统默认部署论文参考优化模型

原因有三点：

- 毕设主线需要与参考文献保持一致，论文撰写时更容易说明“模型来源、算法流程和优化过程”。
- 该模型已经完成 STFT 前处理、ONNX 导出和 C++ 边缘推理链路适配，更符合“边缘医疗监测系统”的整体主题。
- 虽然随机森林在综合指标上略高，但深度模型在论文表达、前处理链路展示、边缘推理流程呈现方面更完整，更适合作为系统主模型；树模型保留为对比实验基线。

需要说明的是：当前实现仍基于 MIT-BIH 单拍 `187` 点输入，不是参考文献中的完整 `10` 秒滑动窗口，因此属于“参考文献方法路线复现与工程化优化”，而不是逐项完全复刻原论文实验环境。

## 当前已实现功能

### 已完成的核心功能

- MIT-BIH 数据分析、清洗、样本准备
- 多模型训练、评估、对比、ONNX 导出
- C++ 边缘推理服务
- C++ 业务服务
- Qt/QML 登录界面与主界面
- 仪表盘指标展示、图表展示、监测历史展示
- 病人管理、报警中心、报警详情展示
- 严重报警弹窗与音效提醒
- 网络超时检测与自动重连
- 密码加密与修改
- SQLite 持久化
- 日志系统
- 香橙派 3 边缘部署联调

### 当前系统主流程

```text
登录
  -> 进入主界面
  -> 查看系统概览与模型指标
  -> 进入实时监测页
  -> 播放样本 / 模拟采集数据
  -> 将心电片段发送到 edge_infer_service
  -> 边缘端返回类别、置信度、告警等级、耗时
  -> 客户端更新波形、状态、历史记录
  -> 若为严重异常，则弹出报警提示并写入报警中心
  -> 业务服务保存监测记录与报警记录
```

## 目录结构

```text
F:\graduation_project
├─ cpp/                           C++ 边缘推理服务、业务服务、网络层、测试
├─ widget/
│  ├─ graduation_project_widget/  当前 Qt/QML 客户端
│  └─ graduation_project_widget_legacy/  归档的旧版 QWidget 客户端
├─ scripts/                       Python 数据处理、训练、对比脚本
├─ shared/                        共享模块，例如日志系统
├─ docs/                          中文技术文档、运行文档、部署文档
├─ MIT-BIH/                       原始数据集（默认不纳入 Git）
├─ processed/                     处理后的训练数据（默认不纳入 Git）
├─ artifacts/                     模型产物（默认不纳入 Git）
├─ results/                       实验结果与联调样本（默认不纳入 Git）
├─ build/                         构建目录（默认不纳入 Git）
└─ doc/                           Word 原始材料（默认不纳入 Git）
```

## 需要自行准备的资源

由于仓库体积和版权原因，以下内容默认不上传到 GitHub：

- `MIT-BIH/` 原始数据集
- `processed/` 处理后数据
- `artifacts/` 训练得到的模型文件（如 `model_best.onnx`）
- `results/qt_real_samples/` 联调用样本
- `third_party/` 大体积第三方依赖
- `build/` 构建产物
- `data/` 运行期数据库和日志

因此，首次克隆仓库后，至少需要你本地准备：

- MIT-BIH 数据集
- ONNX Runtime
- Boost
- SQLite 运行库 / 开发库
- 用于联调的 ONNX 模型文件
- 如需演示播放样本，还需要 `results/qt_real_samples/`

## 开发环境

### Windows 开发环境

- Windows 10 / 11
- Anaconda 环境：`graduation_project`
- Python 3.10+
- Qt 6.x
- Qt Creator 15.0.0 Community
- Visual Studio 2022 Build Tools / Community
- CMake 3.20+
- Boost 1.87.0
- ONNX Runtime

### 香橙派部署环境

- 板卡：Orange Pi 3，2GB
- 系统：Ubuntu / Debian（`aarch64`）
- 编译工具：`cmake`、`g++`
- 推理依赖：ONNX Runtime Linux `aarch64` 版本

## 快速开始

### 1. Python 环境

```powershell
conda activate graduation_project
```

### 2. 构建 C++ 服务端

首次配置：

```powershell
cmake -S F:\graduation_project\cpp -B F:\graduation_project\build\cpp `
  -DONNXRUNTIME_ROOT=你的_onnxruntime目录 `
  -DBOOST_ROOT=你的_boost目录
```

构建边缘推理服务：

```powershell
cmake --build F:\graduation_project\build\cpp --config Release --target edge_infer_service
```

构建业务服务：

```powershell
cmake --build F:\graduation_project\build\cpp --config Release --target business_service
```

### 3. 构建 Qt/QML 客户端

推荐直接用 Qt Creator 打开：

- `widget/graduation_project_widget/CMakeLists.txt`

也可以使用命令行：

```powershell
cmake -S F:\graduation_project\widget\graduation_project_widget `
  -B F:\graduation_project\build\widget_qml
cmake --build F:\graduation_project\build\widget_qml --config Release
```

## 启动顺序

联调时建议按下面顺序启动：

1. 启动 `business_service`
2. 启动 `edge_infer_service`
3. 启动 Qt 客户端

### 1. 启动业务服务

```powershell
F:\graduation_project\build\cpp\Release\business_service.exe `
  --host 0.0.0.0 `
  --port 9200 `
  --data-dir F:\graduation_project\data\business_service `
  --log-dir F:\graduation_project\build\cpp\Release\logs_business_service
```

默认管理员账号：

- 用户名：`admin`
- 密码：`123456`

### 2. 启动边缘推理服务

Windows 本机示例：

```powershell
F:\graduation_project\build\cpp\Release\edge_infer_service.exe `
  --model F:\graduation_project\artifacts\paper_optimized_cnn\model_best.onnx `
  --host 0.0.0.0 `
  --port 9000 `
  --sample-dir F:\graduation_project\results\qt_real_samples
```

香橙派示例：

```bash
./edge_infer_service \
  --model ~/graduation_project/artifacts/paper_optimized_cnn/model_best.onnx \
  --host 0.0.0.0 \
  --port 9000 \
  --sample-dir ~/graduation_project/results/qt_real_samples
```

### 3. 启动 Qt 客户端

可在 Qt Creator 中直接运行 `graduation_project_widget`，也可以运行构建目录中的可执行文件。

客户端连接成功后，可以依次测试：

1. 登录
2. 病人管理
3. 边缘端连接状态
4. 播放样本与实时波形显示
5. 推理结果与历史记录
6. 严重报警弹窗与音效
7. 报警中心记录与确认
8. 密码修改
9. 超时与重连

## 香橙派 3 部署说明

Windows 上修改边缘端代码后，要让香橙派运行最新版本，需要重新同步源码并重新编译。基本步骤如下：

### 1. 同步最新代码

如果板子上已经克隆仓库：

```bash
cd ~/graduation_project
git pull origin main
```

如果不通过 Git，也可以使用 WinSCP / SFTP / SCP 手动覆盖上传以下目录：

- `cpp/`
- `shared/`
- `artifacts/` 中的模型文件
- `results/qt_real_samples/`（如果需要联调样本播放）

### 2. 重新编译

```bash
cd ~/graduation_project
mkdir -p build_orangepi
cd build_orangepi
cmake ../cpp -DONNXRUNTIME_ROOT=~/third_party/onnxruntime-linux-aarch64-1.23.2
cmake --build . --target edge_infer_service -j2
```

Orange Pi 3 为 2GB 内存，建议优先使用：

- `-j2`
- 如果内存紧张，再改为 `-j1`

## 文档索引

更多中文技术文档见：[`docs/`](docs/)

建议优先阅读：

- [`docs/`](docs/) 目录下的论文方法复现、边缘部署、QML 前端架构、报警模块、网络重连等文档
- [`cpp/CMakeLists.txt`](cpp/CMakeLists.txt)
- [`widget/graduation_project_widget/CMakeLists.txt`](widget/graduation_project_widget/CMakeLists.txt)
- [`scripts/`](scripts/)

## 当前阶段说明

当前项目已经不是“只有模型脚本”的阶段，而是已经具备以下完整闭环能力：

- 算法训练
- 模型对比
- ONNX 导出
- 边缘推理
- 业务服务
- 桌面端监测
- 报警联动
- 香橙派部署联调

如果你接下来用于毕业论文撰写，README 对应的项目主线可以概括为：

“以 MIT-BIH 心电数据集为基础，参考文献《Reliable ECG Anomaly Detection on Edge Devices for Internet of Medical Things Applications》的算法路线，设计并实现了一套基于边缘计算的心电异常检测系统。系统采用 Python 完成模型训练与优化，采用 C++ 构建边缘推理与业务服务，采用 Qt/QML 构建监测客户端，并在香橙派 3 上完成边缘部署验证。”



## 自定义 JSON 报文协议

为提升系统在 Qt 客户端、业务服务端和边缘推理服务之间的通信规范性，当前项目已经将原先的按行文本协议升级为“固定包头 + JSON 负载”的自定义应用层协议。

协议结构如下：

```text
[ 魔数 (2 Bytes) ] [ 版本号 (1 Byte) ] [ 消息类型 (2 Bytes) ] [ 负载长度 (4 Bytes) ] [ 负载数据 Payload (N Bytes) ]
| <------------------------- 包头 Header (固定 9 字节) -------------------------> | <------- 包体 Payload -------> |
```

协议要点：

- 魔数固定为 `0x4750`（ASCII: `GP`）
- 版本号当前为 `0x01`
- 字节序统一为大端序
- 包体统一为 JSON 对象
- 推理服务和业务服务均已接入该协议
- Qt 客户端当前也已完成协议同步改造

这样做的好处是：

- 字段语义更清晰，便于后续扩展
- 不再依赖文本分隔符，协议更稳定
- 更适合论文中描述“系统通信协议设计”部分

详细说明见：

- [`docs/自定义JSON报文协议说明.md`](docs/自定义JSON报文协议说明.md)
