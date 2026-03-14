# C++ 推理与网络层说明

当前项目包含两个 C++ 程序：
- `edge_infer`：离线推理与指标统计
- `edge_infer_service`：边缘侧 TCP 推理服务（Boost.Asio）

## 1. 网络分层（参考 GateServer 思路）

`edge_infer_service` 采用以下层次：

1. `IOContextPool`（网络线程池）
- 文件：`src/net/io_context_pool.h/.cpp`
- 作用：管理多个 `io_context`，轮询分配会话线程。

2. `TcpServer`（接入层）
- 文件：`src/net/tcp_server.h/.cpp`
- 作用：监听端口、接收连接、创建会话。

3. `TcpSession`（连接层）
- 文件：`src/net/tcp_session.h/.cpp`
- 作用：按行读取请求、调用逻辑层、回写响应。

4. `LogicSystem`（路由/分发层）
- 文件：`src/logic/logic_system.h/.cpp`
- 作用：命令分发（`PING` / `PREDICT` / `QUIT`）。

5. `InferenceEngine`（业务服务层）
- 文件：`src/service/inference_engine.h/.cpp`
- 作用：调用 ONNX Runtime 完成推理与告警判定。

6. `main`（组装层）
- 文件：`src/edge_infer_service.cpp`
- 作用：解析参数、装配各层并启动服务。

## 2. 协议

每条请求以换行结束：

- `PING`
  - 响应：`PONG`

- `PREDICT f1,f2,...,f187`
  - 成功：`OK pred=<类别> conf=<置信度> alert=<normal|warning|critical> latency_ms=<毫秒>`
  - 失败：`ERR <错误信息>`

- `QUIT`
  - 响应：`BYE`

## 3. 构建与运行

在 Cursor 里直接运行任务：

- `One-Click: Configure+Build+Run edge_infer`
- `One-Click: Configure+Build+Run edge_infer_service`

或命令行：

```powershell
.vscode\configure_edge_infer_v2.cmd
.vscode\build_edge_infer_v2.cmd
.vscode\build_edge_service_v2.cmd
.vscode\run_edge_infer_v2.cmd
.vscode\run_edge_service_v2.cmd
```

## 4. 依赖

- ONNX Runtime：`third_party/onnxruntime-win-x64-1.23.2`
- Boost：`F:/boost_1_87_0`

CMake 与脚本已自动传入 `BOOST_ROOT`。
