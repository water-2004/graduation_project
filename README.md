# 基于边缘设备的心电异常检测系统

## 项目简介

本项目是毕业设计项目，目标是实现一个“基于边缘设备的心电异常检测系统”。系统以 MIT-BIH 心电数据集为基础，使用 Python 完成数据处理与模型训练，将模型导出为 ONNX 后部署到边缘端，最终由 C++/Qt 客户端完成业务管理、监测展示和报警交互。

当前系统的主线链路如下：

```text
MIT-BIH 数据集
  -> Python 数据清洗与模型训练
  -> ONNX 模型导出
  -> C++ 边缘推理服务
  -> C++ 业务服务
  -> Qt 桌面客户端
```

本项目的技术路线是：

- Python 负责数据处理、模型训练、实验对比与 ONNX 导出
- C++ 负责边缘推理服务和业务服务
- Qt 负责桌面端界面与交互
- Boost.Asio 负责服务端与客户端网络通信
- SQLite 负责当前业务数据持久化

## 系统架构

### 1. 训练与模型侧

- 数据集：`MIT-BIH/`
- 数据处理脚本：`scripts/`
- 论文参考模型训练脚本：`scripts/09_train_paper_reference.py`
- 优化版本训练脚本：`scripts/11_train_paper_optimized.py`
- 模型对比脚本：`scripts/12_compare_all_models.py`

当前系统在论文路线下，主模型参考文献《Reliable ECG Anomaly Detection on Edge Devices for Internet of Medical Things Applications》，并围绕该路线做了训练、对比和部署适配。

### 2. 边缘推理侧

边缘端核心目标：

- 加载 ONNX 模型
- 对外提供 TCP 推理服务
- 接收客户端发送的心拍样本
- 返回预测类别、置信度、告警等级和耗时

当前边缘端支持两类模型输入：

1. 一维心拍输入 `[N,1,187]`
2. 论文参考模型输入 `[N,1,17,11]`

当加载论文参考模型时，边缘服务会自动执行：

1. 带通滤波
2. 归一化
3. STFT 频谱变换
4. 构造 `1 x 17 x 11` 张量
5. 调用 ONNX Runtime 推理

这意味着 Qt 客户端无需修改协议，仍然只需要发送 `187` 个原始采样点。

### 3. 业务服务侧

业务服务由 C++ 编写，当前已支持：

- 登录认证
- 病人信息管理
- 监测记录保存
- 报警记录生成、查询与确认
- SQLite 数据持久化
- 与 Qt 客户端的统一业务协议交互

### 4. Qt 客户端侧

Qt 客户端位于 `widget/graduation_project_widget/`，当前主要页面包括：

- 登录页 `LoginPage`
- 仪表盘页 `DashboardPage`
- 病人管理页 `PatientPage`
- 报警中心页 `AlarmCenterPage`
- 推理监测页 `InferPage`
- 关于页 `AboutPage`

当前客户端通信拆分为两条链路：

- 业务链路：Qt -> `business_service`
- 推理链路：Qt -> `edge_infer_service`

## 目录结构

```text
F:\graduation_project
├─ cpp/                           C++ 推理服务、业务服务、网络层与单元测试
├─ widget/                        Qt 客户端工程
├─ scripts/                       Python 数据处理、训练和模型对比脚本
├─ shared/                        共享模块，例如日志系统
├─ docs/                          中文技术文档、运行说明、部署说明
├─ data/                          运行时业务数据目录（默认不纳入 Git）
├─ MIT-BIH/                       原始数据集（默认不纳入 Git）
├─ processed/                     处理后的训练数据（默认不纳入 Git）
├─ artifacts/                     训练得到的模型产物（默认不纳入 Git）
├─ results/                       实验结果与导出样本（默认不纳入 Git）
└─ build/                         构建目录（默认不纳入 Git）
```

## 主要构建目标

### C++ 目标

位于 [cpp/CMakeLists.txt](cpp/CMakeLists.txt)：

- `edge_infer`
  单机命令行推理程序
- `edge_infer_service`
  边缘推理 TCP 服务
- `business_service`
  业务服务端
- `test_sha256`
  SHA-256 单元测试
- `test_business_logic`
  业务逻辑单元测试

### Qt 目标

位于 [widget/graduation_project_widget/CMakeLists.txt](widget/graduation_project_widget/CMakeLists.txt)：

- `graduation_project_widget`
  Qt 桌面客户端

## 关键脚本说明

位于 `scripts/`：

- `01_profile_data.py`
  数据集概览与统计分析
- `02_prepare_mitbih.py`
  MIT-BIH 数据清洗、切分与样本准备
- `03_train_cnn.py`
  基础 CNN 训练脚本
- `04_export_qt_real_samples.py`
  导出 Qt 端联调用真实样本
- `05_train_random_forest.py`
  随机森林基线模型
- `06_train_xgboost.py`
  XGBoost 基线模型
- `07_compare_models.py`
  多模型实验对比
- `08_train_paper_style_cnn.py`
  论文风格模型尝试
- `09_train_paper_reference.py`
  论文参考模型训练脚本
- `10_export_business_data_to_sqlite.py`
  业务侧数据导出/整理脚本
- `11_train_paper_optimized.py`
  论文参考模型优化版训练脚本
- `12_compare_all_models.py`
  多组模型综合对比脚本

## 开发环境

### Windows 开发环境

- Windows 10 / 11
- Anaconda 环境：`graduation_project`
- Python 3.10+
- Qt Creator 15.0.0 Community
- Qt 6.x
- Visual Studio 2022 Build Tools 或 Community
- CMake 3.20+
- Boost 1.87.0
- ONNX Runtime

### 香橙派 3 部署环境

- 板卡：Orange Pi 3，2GB
- 系统：建议使用稳定版 Ubuntu / Debian 系 Linux
- 架构：`aarch64`
- 编译环境：`cmake`、`g++`
- 推理依赖：ONNX Runtime Linux 版

## 快速开始

### 1. Python 环境

如果已经创建过 Anaconda 环境，可以直接激活：

```powershell
conda activate graduation_project
```

### 2. C++ 服务端构建

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

### 3. Qt 客户端构建

推荐直接用 Qt Creator 打开：

- [widget/graduation_project_widget/CMakeLists.txt](widget/graduation_project_widget/CMakeLists.txt)

也可以使用命令行方式：

```powershell
cmake -S F:\graduation_project\widget\graduation_project_widget `
  -B F:\graduation_project\build\widget
cmake --build F:\graduation_project\build\widget --config Release
```

## 启动顺序

联调时建议按下面顺序启动：

1. 香橙派启动 `edge_infer_service`
2. Windows 启动 `business_service`
3. Windows 启动 Qt 客户端

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
  --model F:\graduation_project\artifacts\paper_reference_cnn_full_v1\model_best.onnx `
  --host 0.0.0.0 `
  --port 9000 `
  --sample-dir F:\graduation_project\results\qt_real_samples
```

香橙派示例：

```bash
./edge_infer_service \
  --model ~/graduation_project/artifacts/paper_reference_cnn_full_v1/model_best.onnx \
  --host 0.0.0.0 \
  --port 9000 \
  --sample-dir ~/graduation_project/results/qt_real_samples
```

### 3. Qt 客户端联调

Qt 客户端连接成功后，可以依次测试：

1. 登录
2. 病人增删改查
3. 连接边缘端
4. 播放样本并显示波形
5. 写入监测记录
6. 生成报警并在报警中心查看
7. 报警详情弹窗与确认
8. 修改密码
9. 超时与自动重连

## 香橙派部署更新步骤

当 Windows 上改动了边缘端代码后，要让香橙派运行最新版本，需要重新同步源码并重新编译。建议按下面步骤操作。

### 1. 在 Windows 上确认最新文件

至少需要同步：

- `cpp/`
- `shared/`
- `artifacts/paper_reference_cnn_full_v1/model_best.onnx`
- 如需样本联调，再同步 `results/qt_real_samples/`

### 2. 传到香橙派

如果你已经把仓库放到 GitHub，最简单的方式是板子上执行：

```bash
cd ~/graduation_project
git pull origin main
```

如果暂时不用 GitHub，也可以用 `scp` / `sftp` / `WinSCP` 直接覆盖上传对应目录。

### 3. 在香橙派重新编译

```bash
cd ~/graduation_project
mkdir -p build/cpp
cd build/cpp
cmake ../../cpp -DONNXRUNTIME_ROOT=/path/to/onnxruntime
cmake --build . --config Release -j2
```

Orange Pi 3 为 2GB 内存，建议使用：

- `-j2`
- 或更保守的 `-j1`

### 4. 启动并验证

```bash
./edge_infer_service \
  --model ~/graduation_project/artifacts/paper_reference_cnn_full_v1/model_best.onnx \
  --host 0.0.0.0 \
  --port 9000 \
  --sample-dir ~/graduation_project/results/qt_real_samples
```

验证要点：

- 服务能正常启动
- 日志显示模型已加载
- Qt 可以连上板子 IP 的 `9000` 端口
- 样本预测能返回类别、置信度和报警等级

更完整的联调步骤见：

- [docs/香橙派边缘端部署与联调测试说明.md](docs/香橙派边缘端部署与联调测试说明.md)
- [docs/论文参考模型边缘部署说明.md](docs/论文参考模型边缘部署说明.md)


为了避免仓库过大，以下内容默认不纳入 Git：

- 数据集 `MIT-BIH/`
- 运行时数据 `data/`
- 训练中间结果 `processed/`
- 模型产物 `artifacts/`
- 实验输出 `results/`
- 构建目录 `build/`
- 原始 Word 材料 `doc/`
- 本地工具配置 `.claude/`

因此，GitHub 仓库主要保存：

- 源代码
- Qt 工程
- CMake 工程
- 中文技术文档
- 配置与协议设计


## 文档索引

当前已有的关键文档包括：

- [docs/业务服务端设计说明.md](docs/业务服务端设计说明.md)
- [docs/业务服务端运行说明.md](docs/业务服务端运行说明.md)
- [docs/论文参考模型边缘部署说明.md](docs/论文参考模型边缘部署说明.md)
- [docs/香橙派边缘端部署与联调测试说明.md](docs/香橙派边缘端部署与联调测试说明.md)
- [docs/模拟采集与波形显示说明.md](docs/模拟采集与波形显示说明.md)
- [docs/网络超时与自动重连技术说明.md](docs/网络超时与自动重连技术说明.md)
- [docs/用户密码安全与修改技术说明.md](docs/用户密码安全与修改技术说明.md)
- [docs/监测记录与报警模块运行说明.md](docs/监测记录与报警模块运行说明.md)

## 当前阶段总结

当前项目已经具备以下基础能力：

- 完整的数据处理与模型训练流程
- 论文参考模型训练与 ONNX 导出
- C++ 边缘推理服务
- C++ 业务服务与 SQLite 持久化
- Qt 桌面客户端分层页面结构
- 登录、病人管理、监测记录、报警中心、报警详情、密码修改
- 网络超时与自动重连

