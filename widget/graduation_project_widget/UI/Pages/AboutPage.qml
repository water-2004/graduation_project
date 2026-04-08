import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import graduation_project.widget

Flickable {
    id: root
    property var appViewModel

    contentWidth: width
    contentHeight: column.implicitHeight + 12
    clip: true

    ColumnLayout {
        id: column
        width: root.width
        spacing: 18

        SectionCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            title: "系统定位"
            subtitle: "该毕设采用‘Qt/QML 前端 + C++ 业务服务 + 边缘推理服务 + Python 训练链路’的四层协同结构。"

            RowLayout {
                anchors.fill: parent
                spacing: 16

                Repeater {
                    model: [
                        { title: "前端展示", detail: "QML + C++ ViewModel，负责登录、总览、监测和报警交互", color: Theme.primary },
                        { title: "业务服务", detail: "business_service 负责登录、病人、记录、报警持久化", color: Theme.teal },
                        { title: "边缘推理", detail: "edge_infer_service 负责样本播放与 ONNX 推理", color: Theme.warning },
                        { title: "训练链路", detail: "Python 负责数据处理、模型对比、训练与导出 ONNX", color: Theme.success }
                    ]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 18
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8
                            Rectangle {
                                width: 42
                                height: 6
                                radius: 3
                                color: modelData.color
                            }
                            Label {
                                text: modelData.title
                                color: Theme.text
                                font.bold: true
                            }
                            Label {
                                text: modelData.detail
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                title: "当前登录用户"
                subtitle: "来自业务服务登录会话"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "用户：" + root.appViewModel.currentUserName; color: Theme.text; font.pixelSize: 20; font.bold: true }
                    Label { text: "角色：" + root.appViewModel.currentUserRole; color: Theme.textSecondary }
                    Label { text: "业务状态：" + root.appViewModel.businessStateText; color: Theme.textSecondary }
                    Label { text: "待处理报警：" + root.appViewModel.notificationCount; color: Theme.textSecondary }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                title: "联调说明"
                subtitle: "运行链路建议"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label { text: "1. Windows 启动 business_service"; color: Theme.textSecondary }
                    Label { text: "2. 香橙派启动 edge_infer_service"; color: Theme.textSecondary }
                    Label { text: "3. QML 客户端先登录业务服务，再进入监测页连接边缘端"; color: Theme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "4. 监测结果自动写回业务服务，异常会同步进入报警中心"; color: Theme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
        }
    }
}
