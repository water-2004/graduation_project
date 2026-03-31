# Qt报警中心模块技术说明

## 1. 模块目标

报警中心模块用于把“按病人查看报警”升级为“全局集中查看报警”。

它主要解决三个问题：

1. 医护人员不需要逐个点开病人才能看到异常报警。
2. 可以按病人、级别、状态快速筛选报警。
3. 可以在一个集中页面完成报警确认处理。

当前实现坚持最小可用原则：

1. 全局报警列表由业务服务端统一提供。
2. 筛选逻辑先放在 Qt 客户端本地完成。
3. 报警确认仍复用原有 `CONFIRM_ALERT` 协议，不重新设计一套确认链路。

## 2. 模块结构

本模块新增的核心文件：

- `widget/graduation_project_widget/alarm_center_page.h`
- `widget/graduation_project_widget/alarm_center_page.cpp`

同时联动了以下已有模块：

- `widget/graduation_project_widget/mainwindow.h`
- `widget/graduation_project_widget/mainwindow.cpp`
- `widget/graduation_project_widget/patient_page.h`
- `widget/graduation_project_widget/patient_page.cpp`
- `widget/graduation_project_widget/business_client.h`
- `widget/graduation_project_widget/business_client.cpp`
- `cpp/src/business/file_repository.h`
- `cpp/src/business/file_repository.cpp`
- `cpp/src/business/business_logic.cpp`

## 3. 后端协议设计

### 3.1 新增命令

本次新增全局报警查询命令：

```text
LIST_ALL_ALERTS
```

服务端返回：

```text
ALL_ALERTS alert_id|patient_id|created_at|alert_level|pred_label|confidence|source|sample_name|status|confirmed_at|confirmed_by;...
```

### 3.2 后端实现思路

业务服务端在 SQLite 仓库层新增：

```cpp
std::vector<AlertRecord> ListAllAlerts() const;
```

执行逻辑很直接：

1. 从 `alerts` 表读取全部报警。
2. 按 `created_at DESC, alert_id DESC` 排序。
3. 返回给业务逻辑层。
4. 业务逻辑层序列化为 `ALL_ALERTS ...` 文本响应。

这样做的优点是：

1. 不影响原有 `LIST_ALERTS <patient_id>`。
2. 病人页和报警中心页可以并存。
3. 页面间职责清楚，后续答辩时也容易讲清楚“按病人查看”和“全局查看”的区别。

## 4. Qt 页面设计

### 4.1 页面职责

报警中心页负责：

1. 展示全局报警表格。
2. 提供关键词筛选。
3. 提供告警级别筛选。
4. 提供报警状态筛选。
5. 提供报警确认按钮。
6. 提供返回病人管理页按钮。

### 4.2 表格字段

当前表格展示以下列：

1. 时间
2. 病人编号
3. 病人姓名
4. 级别
5. 预测标签
6. 置信度
7. 来源
8. 样本
9. 状态
10. 确认时间
11. 确认人

说明：

1. 报警记录本身只有 `patient_id`，病人姓名不是后端直接返回的。
2. 病人姓名是 Qt 客户端收到病人列表后，在本地用 `patient_id` 做映射补出来的。

### 4.3 本地筛选策略

筛选没有放到服务端，而是先放在 Qt 本地完成，原因如下：

1. 当前报警量还不大，本地筛选足够快。
2. 这样能减少业务服务端接口复杂度。
3. 毕设阶段先把功能闭环跑通，比过早做复杂查询接口更稳。

当前支持三类筛选：

1. 关键词：支持病人编号、病人姓名、预测标签、样本名。
2. 级别：`全部级别 / critical / warning`
3. 状态：`全部状态 / new / confirmed`

## 5. 页面交互流程

### 5.1 从病人页进入报警中心

病人管理页新增了一个“报警中心”按钮。

点击后，主窗口执行两件事：

1. 切换到 `AlarmCenterPage`
2. 自动触发一次刷新：
   - 拉病人列表
   - 拉全局报警列表

### 5.2 确认报警流程

报警中心页确认报警时，仍复用已有协议：

```text
CONFIRM_ALERT alert_id|confirmed_by
```

处理流程：

1. 用户在表格中选中一条报警。
2. 如果状态已经是 `confirmed`，则禁止重复确认。
3. 如果状态是 `new`，则弹确认框。
4. 点击确认后，客户端发送 `CONFIRM_ALERT`。
5. 服务端更新 SQLite 中的 `alerts` 表。
6. 页面收到 `ALERT_OK` 后自动刷新全局报警列表。

## 6. 设计取舍

### 6.1 为什么不直接把报警中心做进病人页

因为这两种查看方式的目标不一样：

1. 病人页适合看“某个病人的局部报警”。
2. 报警中心适合看“全系统的全局报警”。

如果强行堆在一个页面里，会导致：

1. 页面职责混乱。
2. UI 信息密度过高。
3. 代码也更难解释。

所以当前拆成独立页面更合理。

### 6.2 为什么不先做服务端复杂筛选接口

因为现阶段先做：

1. 全局查询
2. Qt 本地筛选
3. 报警确认

已经足够支撑毕业设计演示。

如果后续老师明确要求更深的数据库能力，再补：

1. 分页
2. 条件查询
3. 时间区间过滤
4. 服务端分页响应

## 7. 当前完成度

当前已经完成：

1. `LIST_ALL_ALERTS` 协议。
2. SQLite 全量报警查询实现。
3. Qt 报警中心独立页面。
4. 病人页进入报警中心入口。
5. 本地关键词 / 级别 / 状态筛选。
6. 全局报警确认。
7. Qt 工程编译通过。
8. 后端协议烟测通过。

## 8. 后续扩展建议

建议后续继续补：

1. 报警按时间区间筛选。
2. 报警分页。
3. 报警详情弹窗。
4. 报警处理状态从 `new / confirmed` 扩展为 `new / confirmed / resolved`。


