# 报警确认与SQLite快照导出运行说明

## 1. 这一步现在能做什么

当前这部分已经能完成三件事：

1. 在 Qt 病人管理页中对报警记录执行“确认报警”。
2. 业务服务端运行时把数据直接写入 SQLite。
3. 如有需要，仍可把旧版 TSV 数据目录离线导出为 SQLite 快照。

## 2. 启动顺序

建议按以下顺序运行：

1. 启动 `business_service`
2. 启动 `edge_infer_service`
3. 启动 Qt 客户端
4. 登录业务服务端
5. 选择病人进入监测页
6. 触发一次异常推理
7. 返回病人页查看并确认报警

## 3. 报警确认功能使用方法

### 3.1 如何触发报警

当前只要推理结果的 `alert_level` 满足以下任意一种，就会自动生成报警记录：

1. `warning`
2. `critical`

你可以通过以下方式触发：

1. 播放 MIT-BIH 异常样本
2. 输入手工特征并得到异常结果
3. 使用你当前模拟采集的样本流

### 3.2 如何确认报警

1. 登录 Qt 客户端。
2. 在病人管理页选择一个病人。
3. 页面右侧会自动加载该病人的报警记录。
4. 在报警记录表中选中一条状态为 `new` 的报警。
5. 点击“确认报警”。
6. 确认成功后，该条记录会更新为：
   - `status = confirmed`
   - `confirmed_at = 当前确认时间`
   - `confirmed_by = 当前登录用户`

## 4. 业务服务端运行数据说明

当前 `business_service` 运行后，业务主数据会落到：

- `business_service.db`

也就是说：

1. 登录数据在 SQLite 中
2. 病人档案在 SQLite 中
3. 监测记录在 SQLite 中
4. 报警记录和确认状态也在 SQLite 中

如果你的数据目录中还有旧版 `TSV` 文件，服务首次启动时会尝试导入，但导入后在线主存储仍然是 `business_service.db`。

## 5. 协议说明

客户端发送：

```text
CONFIRM_ALERT alert_id|confirmed_by
```

服务端成功返回：

```text
ALERT_OK 报警已确认
```

## 6. 我这边已经验证过的结果

我在 `2026-03-26` 完成了一轮实际验证，流程如下：

1. 启动 `business_service`
2. 登录默认管理员 `admin / 123456`
3. 新增病人 `P001`
4. 写入一条 `critical` 监测记录
5. 自动生成报警记录
6. 重启 `business_service`
7. 再次查询病人、监测记录和报警记录

结果摘要：

```text
第一次运行：
ADD_PATIENT => OK 病人已添加
ADD_MONITOR_RECORD => RECORD_OK 监测记录已保存，并生成报警记录
LIST_ALERTS => 能查询到 new 状态报警

第二次运行：
重启后仍能查到病人、监测记录、报警记录
说明 SQLite 持久化已经生效
```

同时，数据目录中实际生成了：

```text
business_service.db
```

## 7. SQLite 快照导出工具什么时候用

脚本位置：

```text
F:\graduation_project\scripts\10_export_business_data_to_sqlite.py
```

现在这个脚本只在以下场景有意义：

1. 你手里还有旧版 TSV 数据目录
2. 你想单独归档一个 SQLite 快照文件
3. 你想在答辩时展示离线整理后的数据库文件

### 7.1 默认导出命令

```powershell
python scripts\10_export_business_data_to_sqlite.py
```

### 7.2 指定数据目录和输出数据库

```powershell
python scripts\10_export_business_data_to_sqlite.py --data-dir F:\graduation_project\data\business_service --output-db F:\graduation_project\artifacts\business_service_snapshot.db
```

## 8. 与老师沟通时建议怎么说

可以这样说：

```text
目前系统已经支持报警确认功能，业务服务端运行时由 C++ 直接接入 SQLite，Qt 客户端通过协议完成登录、病人管理、监测记录写入和报警确认。旧版 TSV 数据如果存在，可以在首次启动时自动导入，Python 主要用于模型训练与数据处理。
```

## 9. 当前不要说错的地方

以下说法现在不准确：

```text
业务服务端当前仍然主要使用 TSV 持久化
```

当前准确说法应该是：

```text
业务服务端当前已经运行在 C++ + SQLite 在线持久化模式下，旧版 TSV 仅作为兼容导入来源，导出脚本仅用于离线迁移和快照整理。
```

## 10. 下一步建议

建议下一步继续做：

1. Qt 报警中心页
2. 病人信息修改接口
3. 报警筛选与时间区间查询
4. 登录密码加密存储
