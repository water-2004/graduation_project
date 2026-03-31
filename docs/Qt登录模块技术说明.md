# Qt 登录模块技术说明

## 1. 模块目标

本模块是系统完善阶段的第二阶段，目标是在 Qt 客户端中增加正式入口页面，使系统不再启动后直接进入监测页，而是先通过业务服务端完成登录认证，再进入实时监测页面。

## 2. 当前实现范围

当前版本已经完成：

1. 登录页面 UI
2. Qt 业务服务端客户端
3. 登录请求发送
4. 登录结果解析
5. 登录成功后进入原有监测页

当前版本暂未完成：

1. 注销登录
2. 多角色界面差异
3. 病人管理页面跳转
4. 登录状态持久化

## 3. 新增文件

### 3.1 登录页面

- [login_page.h](F:/graduation_project/widget/graduation_project_widget/login_page.h)
- [login_page.cpp](F:/graduation_project/widget/graduation_project_widget/login_page.cpp)

职责：

- 显示业务服务端地址、端口、用户名、密码
- 显示登录状态
- 显示登录日志
- 触发登录请求

### 3.2 业务服务端客户端

- [business_client.h](F:/graduation_project/widget/graduation_project_widget/business_client.h)
- [business_client.cpp](F:/graduation_project/widget/graduation_project_widget/business_client.cpp)

职责：

- 连接 `business_service`
- 发送 `LOGIN` 命令
- 解析 `LOGIN_OK` 与 `ERR`
- 保持业务服务端连接，供后续病人管理继续复用

## 4. 修改文件

- [global.h](F:/graduation_project/widget/graduation_project_widget/global.h)
- [mainwindow.h](F:/graduation_project/widget/graduation_project_widget/mainwindow.h)
- [mainwindow.cpp](F:/graduation_project/widget/graduation_project_widget/mainwindow.cpp)
- [main.cpp](F:/graduation_project/widget/graduation_project_widget/main.cpp)
- [CMakeLists.txt](F:/graduation_project/widget/graduation_project_widget/CMakeLists.txt)

## 5. 当前页面结构

当前 Qt 客户端主窗口结构变为：

1. `LoginPage`
2. `InferPage`

主窗口通过 `QStackedWidget` 管理页面切换：

- 启动时显示 `LoginPage`
- 登录成功后切换到 `InferPage`

## 6. 当前登录流程

当前登录链路如下：

1. 用户在 Qt 登录页输入业务服务端地址、端口、用户名、密码
2. 登录页发出登录请求信号
3. `BusinessClient` 连接 `business_service`
4. 连接成功后自动发送 `LOGIN username password`
5. 服务端返回：
   - `LOGIN_OK role=... display_name=...`
   - 或 `ERR ...`
6. Qt 客户端根据结果：
   - 成功则切入监测页
   - 失败则弹出提示并保持在登录页

## 7. 当前默认参数

登录页默认填写：

- 地址：`127.0.0.1`
- 端口：`9200`
- 用户名：`admin`
- 密码：`123456`

这与当前第一阶段 `business_service` 的默认管理员账号保持一致。

## 8. 设计原因

当前没有把登录逻辑塞进原来的 `TcpMgr`，原因是：

1. `TcpMgr` 当前专门负责边缘推理服务协议
2. 业务服务端协议和推理服务协议职责不同
3. 后续病人管理、历史记录等业务功能也需要独立客户端复用

因此新增 `BusinessClient` 是更合理的结构划分。

## 9. 当前限制

当前登录成功后只切入监测页，并未在监测页显示用户信息，也没有实现退出登录。

后续建议继续补：

1. 登录后显示当前用户
2. 增加退出登录按钮
3. 登录成功后加载病人列表页
4. 将监测与病人选择关联起来
