# 边缘端实现说明（香橙派）

## 1. 当前完成度

当前边缘端代码已完成“核心服务骨架 + 推理闭环”：

1. 网络层分层（参考 GateServer）
- `IOContextPool`：会话线程池
- `TcpServer`：接入监听
- `TcpSession`：单连接读写
- `LogicSystem`：命令路由
- `InferenceEngine`：ONNX 推理服务

2. 协议已可用
- `PING -> PONG`
- `PREDICT f1,...,f187 -> OK/ERR`
- `QUIT -> BYE`

3. 本机编译与联调已通过
- `edge_infer_service.exe` 可启动
- `PING/QUIT/PREDICT` 已实测通过

## 2. 代码文件对应关系

- 入口组装：`cpp/src/edge_infer_service.cpp`
- 网络线程池：`cpp/src/net/io_context_pool.h/.cpp`
- TCP 服务：`cpp/src/net/tcp_server.h/.cpp`
- TCP 会话：`cpp/src/net/tcp_session.h/.cpp`
- 命令路由：`cpp/src/logic/logic_system.h/.cpp`
- 推理引擎：`cpp/src/service/inference_engine.h/.cpp`

## 3. 还未完成的“板子端工作”

> 代码骨架完成，不等于上板工作全部结束。以下内容仍需完成：

1. 香橙派环境部署
- 安装/交叉编译 ONNX Runtime（ARM 版本）
- 编译 Boost.Asio 依赖（头文件可直接用，注意编译器版本）
- 用 GCC/Clang 在板子上编译通过

2. 板子实测
- 长时间运行稳定性（至少 30~60 分钟）
- 时延统计（平均/P95）
- 内存占用与 CPU 占用

3. 生产化细节
- 配置文件化（端口、阈值、模型路径）
- 日志落盘与滚动
- 异常重连/超时策略

## 4. 运行方式（Windows 当前环境）

```powershell
.vscode\configure_edge_infer_v2.cmd
.vscode\build_edge_service_v2.cmd
.vscode\run_edge_service_v2.cmd
```

## 5. 给老师汇报可用的一句话

边缘端已完成基于 Boost.Asio 的分层网络服务与 ONNX 推理集成，当前已在本机完成协议联调，下一阶段进入香橙派部署与性能验证。
