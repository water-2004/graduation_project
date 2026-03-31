# 报警确认与SQLite快照导出技术说明

## 1. 本阶段目标

本阶段围绕业务服务端做了两件关键工作：

1. 完成“报警确认”能力。
2. 把业务服务端的运行时持久化正式切换为 `C++ + SQLite`。

同时，之前保留的 `TSV -> SQLite` 导出脚本继续存在，但它现在属于辅助迁移和离线整理工具，不再是运行时主链路。

## 2. 当前整体架构变化

当前系统在这部分的职责划分已经比较清晰：

1. `Python` 负责数据处理、模型训练、模型导出。
2. `edge_infer_service` 负责边缘推理与推理通信。
3. `business_service` 负责登录、病人、监测记录、报警等业务数据管理。
4. `business_service` 运行时直接通过 `SQLite` 持久化业务数据。
5. Qt 客户端通过业务协议访问服务端，不直接操作数据库。

也就是说，现在“报警确认”已经不是写回 `alerts.tsv`，而是直接更新 `business_service.db` 中的 `alerts` 表。

## 3. SQLite 接入方式

本次 C++ 工程已经具备直接链接 SQLite 的能力，工程内新增了：

- `cpp/third_party/sqlite/sqlite3.h`
- `cpp/third_party/sqlite/sqlite3.lib`
- `cpp/third_party/sqlite/sqlite3.dll`

`CMakeLists.txt` 中已经把这些文件接入到 `business_service` 目标：

1. 增加 SQLite 头文件搜索路径
2. 链接 `sqlite3.lib`
3. 构建后自动把 `sqlite3.dll` 复制到可执行文件目录

因此，当前业务服务端运行时不依赖 Python，也不依赖额外手工执行 `sqlite3.exe`。

## 4. 报警确认模块设计

### 4.1 业务含义

报警记录在系统中不应该只有“产生了”这一种状态，还需要能表示“人工已经确认过”。

因此当前引入了报警确认能力，用于表示：

1. 该报警已经被值班人员或管理员确认
2. 系统后续可以区分“新报警”和“已确认报警”

### 4.2 报警记录字段

文件：

- `widget/graduation_project_widget/global.h`
- `cpp/src/business/business_types.h`

报警记录包含以下关键字段：

1. `status`
2. `confirmed_at`
3. `confirmed_by`

含义如下：

1. `status`：当前是否为 `new / confirmed`
2. `confirmed_at`：报警确认时间
3. `confirmed_by`：执行确认操作的用户

### 4.3 协议扩展

文件：`cpp/src/business/business_logic.cpp`

新增命令：

```text
CONFIRM_ALERT alert_id|confirmed_by
```

成功返回：

```text
ALERT_OK 报警已确认
```

失败场景包括：

1. 报警编号不存在
2. 报警已确认，重复操作
3. 参数为空

### 4.4 仓库层处理流程

文件：

- `cpp/src/business/file_repository.h`
- `cpp/src/business/file_repository.cpp`

核心处理流程：

1. 根据 `alert_id` 查询对应报警记录
2. 若不存在则返回错误
3. 若已是 `confirmed` 则拒绝重复确认
4. 更新 `status = confirmed`
5. 自动写入 `confirmed_at` 和 `confirmed_by`
6. 结果直接持久化到 SQLite

## 5. 旧版 TSV 兼容策略

虽然当前运行时已经是 SQLite，但为了兼容前一阶段的数据，本次保留了首次导入逻辑。

策略如下：

1. 服务启动时先创建 SQLite 表结构
2. 如果对应数据表为空，并且同目录下存在旧版 TSV 文件，则自动导入
3. 导入来源包括：
   - `users.tsv`
   - `patients.tsv`
   - `monitor_records.tsv`
   - `alerts.tsv`
4. 导入完成后，后续在线读写统一走 SQLite

这一步的价值是：

1. 不丢历史测试数据
2. 不破坏 Qt 上层协议
3. 让系统平滑从文件存储过渡到数据库存储

## 6. SQLite 快照导出脚本现在的作用

文件：`scripts/10_export_business_data_to_sqlite.py`

这个脚本现在的定位已经变了，它不再是“补偿运行时没有 SQLite”的替代方案，而是一个辅助工具，适合做以下事情：

1. 将旧版独立 TSV 数据目录离线转换为 SQLite 文件
2. 做历史样例归档
3. 给答辩时展示结构化数据库文件
4. 在不启动业务服务端的情况下检查老数据目录

换句话说：

1. 正常运行系统时，不需要依赖这个脚本。
2. 只有在处理老数据、做归档或演示快照时，才需要它。

## 7. 当前完成度

当前已经完成：

1. 报警确认协议
2. Qt 报警确认交互
3. C++ 业务服务端直接链接 SQLite
4. 登录、病人、监测记录、报警记录的 SQLite 持久化
5. 旧版 TSV 首次导入兼容
6. `business_service` 编译通过
7. 冒烟验证通过

## 8. 与老师沟通时建议怎么说

现在可以准确这样表述：

```text
当前业务服务端已经由 C++ 直接接入 SQLite，报警确认、病人管理和监测记录都属于在线数据库持久化；同时保留了对旧版 TSV 数据的首次导入兼容。Python 主要用于模型训练与数据处理，不参与系统运行阶段的业务存储。
```

下面这种说法已经不准确：

```text
当前运行时仍然采用 TSV 持久化，SQLite 只是快照导出成果。
```

## 9. 后续建议

建议下一步继续做：

1. 报警状态从 `new / confirmed` 扩展为 `new / confirmed / resolved`
2. 增加病人信息修改接口
3. 增加时间区间筛选、报警级别筛选
4. 如果后续规模扩大，再评估 MySQL 或 Redis 的必要性
