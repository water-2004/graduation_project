import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import graduation_project.widget

Flickable {
    id: root
    property var viewModel

    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight + 12

    ColumnLayout {
        id: column
        width: root.width
        spacing: 18

        GridLayout {
            Layout.fillWidth: true
            columns: width > 1200 ? 4 : 2
            rowSpacing: 16
            columnSpacing: 16

            MetricCard {
                Layout.fillWidth: true
                title: "总病人数"
                useAnimatedNumber: true
                targetValue: root.viewModel ? root.viewModel.patientCount : 0
                subtitle: "业务端当前可管理的病人档案数"
                accentColor: Theme.primary
                watermark: "P"
                animationDelay: 0
            }
            MetricCard {
                Layout.fillWidth: true
                title: "监测记录"
                useAnimatedNumber: true
                targetValue: root.viewModel ? root.viewModel.recordCount : 0
                subtitle: "累计落库的监测与推理记录"
                accentColor: Theme.teal
                watermark: "R"
                animationDelay: 70
            }
            MetricCard {
                Layout.fillWidth: true
                title: "未处理报警"
                useAnimatedNumber: true
                targetValue: root.viewModel ? root.viewModel.unconfirmedAlertCount : 0
                subtitle: "仍待人工确认与归档的报警数量"
                accentColor: Theme.warning
                watermark: "!"
                animationDelay: 140
            }
            MetricCard {
                Layout.fillWidth: true
                title: "模型准确率"
                useAnimatedNumber: true
                targetValue: root.viewModel ? root.viewModel.modelAccuracy * 100.0 : 0
                decimals: 2
                suffix: "%"
                subtitle: "论文参考模型当前测试精度"
                accentColor: Theme.success
                watermark: "AI"
                animationDelay: 210
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredWidth: root.width * 0.58
                Layout.preferredHeight: 320
                title: "近期报警趋势"
                subtitle: "最近 7 天报警数量变化"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    LineChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: root.viewModel ? root.viewModel.trendPoints : []
                        lineColor: Theme.primary
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: root.viewModel ? root.viewModel.trendPoints : []
                            delegate: ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Label {
                                    text: modelData.label
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.value
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredWidth: root.width * 0.42
                Layout.preferredHeight: 320
                title: "模型性能雷达"
                subtitle: "Accuracy、Macro-F1、Macro-Recall 与各类别召回率"

                RadarChart {
                    anchors.fill: parent
                    metrics: root.viewModel ? root.viewModel.radarMetrics : []
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "识别类别分布"
                subtitle: "基于已保存监测记录的预测类别统计"

                DonutChart {
                    anchors.fill: parent
                    segments: root.viewModel ? root.viewModel.classDistribution : []
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "最新动态"
                subtitle: "最近 5 条监测或报警事件"

                ListView {
                    anchors.fill: parent
                    spacing: 12
                    model: root.viewModel ? root.viewModel.timelineModel : null
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 72
                        radius: 16
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Rectangle {
                                width: 10
                                Layout.fillHeight: true
                                radius: 5
                                color: Theme.levelColor(level)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: title
                                    color: Theme.text
                                    font.bold: true
                                }
                                Label {
                                    text: detail
                                    color: Theme.textSecondary
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }

                            Label {
                                text: time
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}
