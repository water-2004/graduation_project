# 自定义 JSON 报文协议说明

## 1. 协议目标

本项目原先使用按行文本协议（例如 `PING`、`LOGIN user pw`、`PREDICT 1,2,3,...`）。
为提升协议的结构化程度、可扩展性和跨端一致性，现统一升级为：

- 固定 9 字节二进制包头
- JSON 作为包体负载格式
- 大端序编码

该协议同时适用于：

- 边缘推理服务 `edge_infer_service`
- 业务服务 `business_service`
- Qt 客户端

## 2. 报文结构

```text
[ 魔数 (2 Bytes) ] [ 版本号 (1 Byte) ] [ 消息类型 (2 Bytes) ] [ 负载长度 (4 Bytes) ] [ 负载数据 Payload (N Bytes) ]
| <------------------------- 包头 Header (固定 9 字节) -------------------------> | <------- 包体 Payload -------> |
```

## 3. 包头定义

| 字段名 | 长度 | 说明 |
| --- | --- | --- |
| 魔数 | 2 字节 | 固定为 `0x4750`，ASCII 对应 `GP` |
| 版本号 | 1 字节 | 当前版本固定为 `0x01` |
| 消息类型 | 2 字节 | 标识本次请求或响应的业务类型 |
| 负载长度 | 4 字节 | 表示 JSON 包体长度，单位为字节 |

补充说明：

- 字节序统一采用大端序。
- 当前负载最大长度限制为 `4 MB`。
- 当魔数、版本号或长度不合法时，服务端会返回 `ErrorResponse`，并可主动断开连接。

## 4. 包体格式

包体统一使用 JSON 对象。

示例：

```json
{
  "features": [0.123, -0.532, 1.246]
}
```

若某类消息无需额外参数，也统一发送空对象：

```json
{}
```

## 5. 消息类型定义

### 5.1 通用消息

| 名称 | 值 | 说明 |
| --- | --- | --- |
| `PingRequest` | `0x0001` | 心跳请求 |
| `PingResponse` | `0x8001` | 心跳响应 |
| `QuitRequest` | `0x0002` | 主动断开请求 |
| `ByeResponse` | `0x8002` | 断开确认响应 |
| `ErrorResponse` | `0x8FFF` | 错误响应 |

### 5.2 边缘推理服务消息

| 名称 | 值 | 说明 |
| --- | --- | --- |
| `PredictRequest` | `0x0101` | 推理请求 |
| `PredictResponse` | `0x8101` | 推理结果响应 |
| `ListSamplesRequest` | `0x0102` | 获取样本列表 |
| `ListSamplesResponse` | `0x8102` | 返回样本列表 |
| `PlaySampleRequest` | `0x0103` | 播放单个样本 |
| `PlaySampleResponse` | `0x8103` | 返回样本波形与推理结果 |

### 5.3 业务服务消息

| 名称 | 值 | 说明 |
| --- | --- | --- |
| `LoginRequest` | `0x0201` | 登录请求 |
| `LoginResponse` | `0x8201` | 登录成功响应 |
| `ListPatientsRequest` | `0x0202` | 查询病人列表 |
| `ListPatientsResponse` | `0x8202` | 病人列表响应 |
| `GetPatientRequest` | `0x0203` | 查询单个病人 |
| `GetPatientResponse` | `0x8203` | 单个病人详情响应 |
| `AddPatientRequest` | `0x0204` | 新增病人 |
| `AddPatientResponse` | `0x8204` | 新增病人结果 |
| `UpdatePatientRequest` | `0x0205` | 修改病人 |
| `UpdatePatientResponse` | `0x8205` | 修改病人结果 |
| `DeletePatientRequest` | `0x0206` | 删除病人 |
| `DeletePatientResponse` | `0x8206` | 删除病人结果 |
| `AddMonitorRecordRequest` | `0x0207` | 新增监测记录 |
| `AddMonitorRecordResponse` | `0x8207` | 新增监测记录结果 |
| `ListMonitorRecordsRequest` | `0x0208` | 查询指定病人的监测记录 |
| `ListMonitorRecordsResponse` | `0x8208` | 监测记录响应 |
| `ListAllMonitorRecordsRequest` | `0x0209` | 查询全部监测记录 |
| `ListAllMonitorRecordsResponse` | `0x8209` | 全部监测记录响应 |
| `ListAlertsRequest` | `0x020A` | 查询指定病人的报警记录 |
| `ListAlertsResponse` | `0x820A` | 报警记录响应 |
| `ListAllAlertsRequest` | `0x020B` | 查询全部报警记录 |
| `ListAllAlertsResponse` | `0x820B` | 全部报警记录响应 |
| `ConfirmAlertRequest` | `0x020C` | 确认报警 |
| `ConfirmAlertResponse` | `0x820C` | 确认报警结果 |
| `ChangePasswordRequest` | `0x020D` | 修改密码 |
| `ChangePasswordResponse` | `0x820D` | 修改密码结果 |

## 6. 请求与响应示例

### 6.1 心跳请求

请求类型：`PingRequest`

```json
{}
```

响应类型：`PingResponse`

```json
{
  "message": "PONG"
}
```

### 6.2 推理请求

请求类型：`PredictRequest`

```json
{
  "features": [0.12, -0.36, 0.55, 0.87]
}
```

响应类型：`PredictResponse`

```json
{
  "pred_label": "V",
  "confidence": "0.946821",
  "alert_level": "critical",
  "latency_ms": "3.214"
}
```

说明：

- `features` 为长度 187 的特征数组。
- 当前服务端内部仍复用了原命令式推理逻辑，因此最终响应字段与历史语义保持一致。

### 6.3 样本列表请求

请求类型：`ListSamplesRequest`

```json
{}
```

响应类型：`ListSamplesResponse`

```json
{
  "samples": [
    "label_0_sample_1_idx_0",
    "label_2_sample_1_idx_18674"
  ]
}
```

### 6.4 播放样本请求

请求类型：`PlaySampleRequest`

```json
{
  "sample_name": "label_0_sample_1_idx_0"
}
```

响应类型：`PlaySampleResponse`

```json
{
  "sample_name": "label_0_sample_1_idx_0",
  "true_label": "N",
  "pred_label": "N",
  "confidence": "0.998712",
  "alert_level": "normal",
  "latency_ms": "2.441",
  "values": [0.01, 0.03, 0.05, 0.02]
}
```

### 6.5 登录请求

请求类型：`LoginRequest`

```json
{
  "username": "doctor",
  "password": "123456"
}
```

响应类型：`LoginResponse`

```json
{
  "username": "doctor",
  "role": "doctor",
  "display_name": "张医生"
}
```

### 6.6 查询病人列表请求

请求类型：`ListPatientsRequest`

```json
{}
```

响应类型：`ListPatientsResponse`

```json
{
  "patients": [
    {
      "patient_id": "P001",
      "name": "王明",
      "gender": "男",
      "age": "68",
      "phone": "13800000000",
      "remark": "高血压"
    }
  ]
}
```

说明：

- 由于当前服务端 JSON 序列化基于 `boost::property_tree`，部分数值字段在输出时可能表现为字符串，这是当前实现的已知特征。
- Qt 客户端已兼容“数字或字符串”两种形式。

### 6.7 错误响应

响应类型：`ErrorResponse`

```json
{
  "code": "bad_request",
  "message": "缺少字段: patient_id"
}
```

常见错误码示例：

- `invalid_header`
- `invalid_json`
- `bad_request`
- `unsupported_type`
- `login_failed`
- `predict_failed`

## 7. 当前实现策略说明

为了减少对既有系统的冲击，本次协议升级采用了“外层协议升级、内层业务尽量复用”的方式：

- 网络层统一改为固定包头 + JSON 包体。
- `BusinessLogic` 与 `LogicSystem` 新增 `HandlePacket(...)` 接口。
- 业务核心仍尽量复用原先的命令式逻辑，例如 `LOGIN`、`LIST_PATIENTS`、`PREDICT` 等旧流程。

这样做的好处有三点：

- 改动小，风险低。
- 便于逐步迁移，不会一次性重写所有业务。
- 方便论文中说明“系统经历了从文本协议到结构化协议的优化过程”。

## 8. 对论文和答辩的表述建议

你可以这样描述这部分设计：

> 为提高系统在多端协作场景下的可扩展性与协议规范性，本文将原有的文本命令协议升级为固定包头与 JSON 负载结合的自定义应用层协议。该协议通过魔数、版本号、消息类型与负载长度对数据包进行统一描述，解决了文本协议可扩展性不足、字段语义不清晰以及跨端解析不一致的问题。同时，在实现层面保留原有业务处理核心，通过协议适配器完成平滑迁移，从而兼顾了工程可维护性与开发效率。
