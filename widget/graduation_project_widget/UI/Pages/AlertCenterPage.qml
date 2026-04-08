import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import "../Components"
import graduation_project.widget

Item {
    id: root
    property var appViewModel

    RowLayout {
        anchors.fill: parent
        spacing: 18

        SectionCard {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.34
            title: "待处置报警"
            subtitle: "左侧列表负责分诊，优先处理 critical 与未确认项目。"

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                AppTextField {
                    Layout.fillWidth: true
                    placeholderText: "搜索病人编号、预测类别或样本名"
                    onTextChanged: root.appViewModel.setAlertKeyword(text)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    AppComboBox {
                        Layout.fillWidth: true
                        model: [
                            { text: "全部级别", value: "all" },
                            { text: "严重", value: "critical" },
                            { text: "预警", value: "warning" },
                            { text: "正常", value: "normal" }
                        ]
                        textRole: "text"
                        onActivated: root.appViewModel.setAlertLevelFilter(model[currentIndex].value)
                    }

                    AppComboBox {
                        Layout.fillWidth: true
                        model: [
                            { text: "全部状态", value: "all" },
                            { text: "未确认", value: "pending" },
                            { text: "已确认", value: "confirmed" }
                        ]
                        textRole: "text"
                        onActivated: root.appViewModel.setAlertStatusFilter(model[currentIndex].value)
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 12
                    model: root.appViewModel.alertManager.model

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 110
                        radius: 18
                        color: ListView.isCurrentItem ? Theme.levelBackground(alertLevel) : Theme.surfaceAlt
                        border.width: 1
                        border.color: ListView.isCurrentItem ? Theme.levelColor(alertLevel) : Theme.border
                        property bool hovered: false

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                            onClicked: {
                                ListView.view.currentIndex = index
                                root.appViewModel.selectAlert(index)
                            }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: patientId
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                StatusPill { text: alertLevel; level: alertLevel }
                            }

                            Label {
                                text: predLabel + " · " + confidence
                                color: Theme.textSecondary
                            }
                            Label {
                                text: sampleName.length > 0 ? sampleName : source
                                color: Theme.textMuted
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: createdAt
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "报警详情"
            subtitle: "右侧查看完整信息并执行确认归档。"

            ColumnLayout {
                anchors.fill: parent
                spacing: 16

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 190
                    radius: 20
                    color: Theme.levelBackground(root.appViewModel.alertManager.selectedAlert.alertLevel || "normal")
                    border.width: 1
                    border.color: Theme.levelColor(root.appViewModel.alertManager.selectedAlert.alertLevel || "normal")

                    Behavior on color {
                        ColorAnimation { duration: 160 }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: root.appViewModel.alertManager.selectedAlert.patientId || "未选择报警"
                                color: Theme.text
                                font.pixelSize: 26
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            StatusPill {
                                text: root.appViewModel.alertManager.selectedAlert.alertLevel || "normal"
                                level: root.appViewModel.alertManager.selectedAlert.alertLevel || "normal"
                            }
                        }

                        Label {
                            text: (root.appViewModel.alertManager.selectedAlert.predLabel || "等待选择") +
                                  " · " + (root.appViewModel.alertManager.selectedAlert.confidence || "--")
                            color: Theme.textSecondary
                        }

                        Label {
                            text: "触发时间：" + (root.appViewModel.alertManager.selectedAlert.createdAt || "--")
                            color: Theme.textSecondary
                        }

                        Label {
                            text: "来源样本：" + (root.appViewModel.alertManager.selectedAlert.sampleName || root.appViewModel.alertManager.selectedAlert.source || "--")
                            color: Theme.textSecondary
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: 18
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8
                            Label { text: "模型预测详情"; color: Theme.text; font.bold: true }
                            Label { text: "预测标签：" + (root.appViewModel.alertManager.selectedAlert.predLabel || "--"); color: Theme.textSecondary }
                            Label { text: "置信度：" + (root.appViewModel.alertManager.selectedAlert.confidence || "--"); color: Theme.textSecondary }
                            Label { text: "状态：" + (root.appViewModel.alertManager.selectedAlert.status || "--"); color: Theme.textSecondary }
                            Label {
                                text: "确认人：" + (root.appViewModel.alertManager.selectedAlert.confirmedBy || "未确认")
                                color: Theme.textSecondary
                            }
                            Label {
                                text: "确认时间：" + (root.appViewModel.alertManager.selectedAlert.confirmedAt || "--")
                                color: Theme.textSecondary
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: 18
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 8
                            Label { text: "处置建议"; color: Theme.text; font.bold: true }
                            Label {
                                text: root.appViewModel.alertManager.selectedAlert.alertLevel === "critical"
                                      ? "建议立即通知值班人员，对病人进行人工复核。"
                                      : root.appViewModel.alertManager.selectedAlert.alertLevel === "warning"
                                        ? "建议结合历史记录二次判断，并继续观察。"
                                        : "当前仅作归档与留痕。"
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "该区域后续可继续扩展为病历摘要、医生备注与联动消息服务。"
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                AppButton {
                    Layout.fillWidth: true
                    implicitHeight: 54
                    text: "确认并归档报警"
                    tone: "primary"
                    enabled: (root.appViewModel.alertManager.selectedAlert.alertId || "").length > 0
                    onClicked: root.appViewModel.confirmSelectedAlert()
                }
            }
        }
    }
}
