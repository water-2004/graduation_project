# Dashboard 动效补充说明

## 本轮新增效果

针对 Dashboard 页面，本轮新增了两类前端动效：

- 指标卡数字滚动
- 图表入场动画

## 1. 指标卡数字滚动

文件：

- `UI/Components/MetricCard.qml`
- `UI/Pages/DashboardPage.qml`

当前实现方式：

- `MetricCard.qml` 新增了 `useAnimatedNumber`
- 新增了 `targetValue`
- 新增了 `displayedValue`
- 使用 `NumberAnimation` 驱动数字从旧值滚动到目标值
- 支持整数和百分比两种显示形式

在 Dashboard 中已经接入：

- 总病人数
- 监测记录数
- 未处理报警数
- 模型准确率

其中模型准确率采用：

- 数值乘以 `100`
- 保留 `2` 位小数
- 后缀为 `%`

## 2. 图表入场动画

### 2.1 折线图动画

文件：`UI/Components/LineChart.qml`

实现思路：

- 新增 `animationProgress`
- 使用 `NumberAnimation` 将进度从 `0` 动画到 `1`
- 通过 `Canvas` 裁剪区域逐步揭示折线
- 同时保留底部淡色填充区，增强现代感

效果：

- 曲线从左到右逐步绘制出来

### 2.2 环形图动画

文件：`UI/Components/DonutChart.qml`

实现思路：

- 每个分段的弧长乘以 `animationProgress`
- 环形图从起始状态逐步展开
- 图例透明度跟随动画进度变化
- 中心增加总量展示

效果：

- 图表更像真正的数据卡片，而不是静态图形

### 2.3 雷达图动画

文件：`UI/Components/RadarChart.qml`

实现思路：

- 多边形顶点值乘以 `animationProgress`
- 雷达区域从中心向外展开
- 指标文字标签透明度同步变化

效果：

- 雷达图更有“模型性能加载展示”的感觉

## 3. Dashboard 页面接入

文件：`UI/Pages/DashboardPage.qml`

已完成：

- 四张指标卡启用数字滚动
- 四张指标卡增加轻微错峰入场延时
- 折线图接入绘制动画
- 环形图接入展开动画
- 雷达图接入展开动画

## 4. 这轮优化的意义

这轮改动的重点不是增加业务功能，而是：

- 增强首页第一眼的展示力
- 提高系统的现代感和完成度
- 让老师或答辩老师一打开系统，就能明显感受到页面不是“纯静态堆控件”

## 5. 当前验证结果

已经完成：

- QML 编译通过
- 客户端在注入 Qt 运行库路径后可正常启动

说明：

- 代码本身没有问题
- 如果直接双击 `exe` 仍报错，问题还是 Qt 运行库部署，不是这轮动画代码导致的
