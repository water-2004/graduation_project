# Qt病人管理模块技术说明

## 1. 模块目标

病人管理模块位于 Qt 客户端中，处于登录模块与心电监测模块之间，主要承担以下职责：

1. 展示业务服务端保存的病人列表。
2. 支持新增病人基础信息。
3. 支持编辑已有病人信息。
4. 支持删除病人记录。
5. 选择目标病人后进入监测页面。
6. 为后续历史记录、报警记录、病人详情扩展预留入口。

当前模块采用“最小可用”实现，先把业务流程打通，不在本阶段引入复杂权限和数据库界面逻辑。

## 2. 页面流转

当前 Qt 客户端页面流转如下：

1. 登录页 `LoginPage`
2. 病人管理页 `PatientPage`
3. 心电监测页 `InferPage`

具体流程为：

1. 用户在登录页输入业务服务端地址、账号、密码。
2. `BusinessClient` 与业务服务端建立 TCP 连接并发送 `LOGIN`。
3. 登录成功后，主窗口切换到病人管理页。
4. 病人管理页自动发送 `LIST_PATIENTS` 拉取病人列表。
5. 用户可新增、修改、删除病人，或者选择病人进入监测页。
6. 进入监测页后，顶部显示当前监测病人的基础信息。

## 3. 关键文件说明

### 3.1 数据结构

文件：`widget/graduation_project_widget/global.h`

新增结构体：

- `LoginUserInfo`：保存登录成功后的用户名、角色、显示名。
- `PatientInfo`：保存病人编号、姓名、性别、年龄、电话、备注。
- `BeatResponse`：监测服务返回的完整单拍数据结构。

作用：

1. 统一 Qt 页面层和网络层之间的数据表达。
2. 避免业务字段在多个页面中重复定义。
3. 便于信号槽直接传递结构体对象。

### 3.2 业务客户端

文件：

- `widget/graduation_project_widget/business_client.h`
- `widget/graduation_project_widget/business_client.cpp`

职责：

1. 维护 Qt 客户端到业务服务端的 TCP 长连接。
2. 负责发送登录、病人列表、病人新增、病人修改、病人删除等命令。
3. 解析服务端返回的文本协议。
4. 将结果转换为 Qt 页面层可直接使用的结构体和信号。

当前支持命令：

- `LOGIN <username> <password>`
- `LIST_PATIENTS`
- `GET_PATIENT <patient_id>`
- `ADD_PATIENT id|name|gender|age|phone|remark`
- `UPDATE_PATIENT id|name|gender|age|phone|remark`
- `DELETE_PATIENT <patient_id>`
- `QUIT`

当前新增的主要信号：

- `sig_patient_list_ready`
- `sig_patient_detail_ready`
- `sig_patient_operation_success`
- `sig_business_error`

### 3.3 病人管理页

文件：

- `widget/graduation_project_widget/patient_page.h`
- `widget/graduation_project_widget/patient_page.cpp`

职责：

1. 展示病人表格。
2. 提供新增病人的表单。
3. 提供编辑已有病人的表单。
4. 提供删除病人按钮。
5. 记录业务日志。
6. 发出“进入监测”信号。

当前页面分为五块：

1. 标题与说明区。
2. 当前登录用户与当前选中病人信息区。
3. 病人列表区。
4. 病人表单区（新增 / 修改共用）。
5. 业务日志区。

本次编辑模式的关键设计：

1. 选中病人表格行后，表单自动切换到编辑模式。
2. 编辑模式下 `patient_id` 只读，避免误改主键。
3. 点击“清空表单”后回到新增模式。
4. “新增病人”和“更新病人”共用一套表单字段，减少重复 UI。

### 3.4 主窗口调度

文件：

- `widget/graduation_project_widget/mainwindow.h`
- `widget/graduation_project_widget/mainwindow.cpp`

职责：

1. 持有 `LoginPage`、`PatientPage`、`InferPage` 三个页面。
2. 通过 `QStackedWidget` 控制页面切换。
3. 将 `BusinessClient` 与页面信号槽绑定起来。
4. 在选中病人后把 `PatientInfo` 传入监测页。

### 3.5 监测页增强

文件：

- `widget/graduation_project_widget/inferpage.h`
- `widget/graduation_project_widget/inferpage.cpp`

本次新增内容：

1. 顶部增加“当前监测对象”信息区。
2. 增加“返回病人列表”按钮。
3. 支持展示当前进入监测页的病人信息。

这样做的目的是让监测页具备明确的业务上下文，避免界面只显示波形而没有病人归属信息。

## 4. 协议与数据流

### 4.1 登录阶段

Qt 客户端发送：

```text
LOGIN admin 123456
```

服务端返回：

```text
LOGIN_OK role=admin display_name=系统管理员
```

### 4.2 拉取病人列表

Qt 客户端发送：

```text
LIST_PATIENTS
```

服务端返回示例：

```text
PATIENTS P001|张三|男|26|13800000000|实验样本;P002|李四|女|31|13900000000|复诊观察
```

### 4.3 新增病人

Qt 客户端发送：

```text
ADD_PATIENT P003|王五|男|45|13600000000|高风险监测
```

服务端返回：

```text
OK 病人已添加
```

### 4.4 修改病人

Qt 客户端发送：

```text
UPDATE_PATIENT P003|王五-复诊|男|46|13600000001|复诊后更新
```

服务端返回：

```text
OK 病人信息已更新
```

### 4.5 删除病人

Qt 客户端发送：

```text
DELETE_PATIENT P003
```

服务端返回：

```text
OK 病人已删除
```

## 5. 设计原因

### 5.1 为什么先做病人管理，再做更复杂的业务后台

原因有三点：

1. 毕设系统首先需要有完整业务闭环，而不是只有模型推理。
2. 病人管理是后续历史记录、报警记录、设备绑定的基础。
3. 当前先把接口和页面边界稳定下来，后续扩展更复杂的数据库能力或切换到 MySQL 会更容易。

### 5.2 为什么登录和病人管理共用一个 `BusinessClient`

原因如下：

1. 登录和病人管理都属于业务服务端职责。
2. 复用同一条 TCP 连接，代码更简单。
3. 后续若增加查询历史记录、报警记录，也可以继续复用该连接。

### 5.3 为什么编辑模式下不允许直接修改病人编号

原因如下：

1. `patient_id` 是业务主键，也是监测记录与报警记录的关联字段。
2. 当前系统已经有外键关联，直接改主键会引入更多联动逻辑。
3. 对毕业设计阶段来说，先保证“可稳定修改其他字段”比贸然改主键更稳妥。

## 6. 当前完成度

当前病人管理模块已完成：

1. 登录成功后自动进入病人管理页。
2. 可从业务服务端拉取病人列表。
3. 可新增病人。
4. 可编辑病人。
5. 可删除病人。
6. 可选择病人并进入监测页。
7. 监测页可显示当前病人信息。
8. Qt 工程已编译通过。

## 7. 后续可扩展点

建议后续按下面顺序继续扩展：

1. 增加病人列表检索、筛选与分页。
2. 增加“病人详情页”。
3. 增加“历史监测记录”查询功能。
4. 增加“报警记录”列表与确认处理功能。
5. 后续如果导师明确要求，再评估是否需要上 MySQL。
