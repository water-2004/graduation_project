pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import "../Theme.js" as Theme
import "../Components"
import graduation_project.widget

Flickable {
    id: root
    property var viewModel

    clip: true
    contentWidth: width
    contentHeight: pageColumn.implicitHeight + 12

    ColumnLayout {
        id: pageColumn
        width: root.width
        spacing: 18

        Rectangle {
            Layout.fillWidth: true
            radius: 22
            color: "#E0F2FE"
            border.width: 1
            border.color: "#BFDBFE"
            implicitHeight: 108

            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 16

                Rectangle {
                    Layout.preferredWidth: 62
                    Layout.preferredHeight: 62
                    radius: 20
                    color: Theme.primary
                    Label {
                        anchors.centerIn: parent
                        text: root.viewModel && root.viewModel.currentPatientName.length > 0 ? root.viewModel.currentPatientName.charAt(0) : "P"
                        color: "white"
                        font.pixelSize: 26
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: root.viewModel ? root.viewModel.currentPatientName : "未选择病人"
                        color: Theme.text
                        font.pixelSize: 24
                        font.bold: true
                    }
                    Label {
                        text: root.viewModel ? root.viewModel.currentPatientSummary : ""
                        color: Theme.textSecondary
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }

                StatusPill {
                    text: root.viewModel ? root.viewModel.edgeStateText : "边缘服务未连接"
                    level: root.viewModel ? root.viewModel.edgeState : "disconnected"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredWidth: root.width * 0.7
                Layout.preferredHeight: 660
                title: "实时心电波形"
                subtitle: "深色沉浸式示波器风格，支持样本播放、暂停回放与手动推理。"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 320
                        radius: 18
                        color: Theme.monitorDark
                        border.width: 1
                        border.color: "#111827"

                        WaveCanvas {
                            anchors.fill: parent
                            anchors.margins: 10
                            points: root.viewModel ? root.viewModel.ecgStream.points : []
                            alertLevel: root.viewModel ? root.viewModel.ecgStream.alertLevel : "normal"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 94
                        radius: 18
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                AppButton {
                                    text: root.viewModel && root.viewModel.playing ? "暂停回放" : "播放回放"
                                    tone: root.viewModel && root.viewModel.playing ? "secondary" : "primary"
                                    enabled: root.viewModel && root.viewModel.playbackAvailable
                                    onClicked: if (root.viewModel) root.viewModel.togglePlayback()
                                }

                                AppButton {
                                    text: "停止"
                                    tone: "secondary"
                                    enabled: root.viewModel && root.viewModel.playbackAvailable
                                    onClicked: if (root.viewModel) root.viewModel.stopPlayback()
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.viewModel ? root.viewModel.currentPlaybackName : "等待波形回放"
                                    color: Theme.text
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: root.viewModel ? root.viewModel.playbackProgressText : "0 / 0"
                                    color: Theme.textSecondary
                                    font.bold: true
                                }
                            }

                            Slider {
                                id: playbackSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                enabled: root.viewModel && root.viewModel.playbackAvailable

                                Binding {
                                    target: playbackSlider
                                    property: "value"
                                    value: root.viewModel ? root.viewModel.playbackProgress : 0
                                    when: !playbackSlider.pressed
                                }

                                onMoved: if (root.viewModel) root.viewModel.seekPlayback(value)

                                background: Rectangle {
                                    x: playbackSlider.leftPadding
                                    y: playbackSlider.topPadding + playbackSlider.availableHeight / 2 - height / 2
                                    width: playbackSlider.availableWidth
                                    height: 6
                                    radius: 3
                                    color: Theme.border

                                    Rectangle {
                                        width: playbackSlider.visualPosition * parent.width
                                        height: parent.height
                                        radius: 3
                                        color: Theme.primary
                                    }
                                }

                                handle: Rectangle {
                                    x: playbackSlider.leftPadding + playbackSlider.visualPosition * (playbackSlider.availableWidth - width)
                                    y: playbackSlider.topPadding + playbackSlider.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: playbackSlider.pressed ? Theme.teal : Theme.primary
                                    border.width: 2
                                    border.color: "white"
                                }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 12
                        rowSpacing: 12

                        AppTextField {
                            Layout.fillWidth: true
                            text: root.viewModel ? root.viewModel.edgeHost : "127.0.0.1"
                            placeholderText: "边缘主机"
                            onEditingFinished: if (root.viewModel) root.viewModel.edgeHost = text
                        }

                        AppTextField {
                            Layout.fillWidth: true
                            text: root.viewModel ? root.viewModel.edgePort : 9000
                            validator: IntValidator { bottom: 1; top: 65535 }
                            placeholderText: "端口"
                            onEditingFinished: if (root.viewModel) root.viewModel.edgePort = Number(text)
                        }

                        AppButton {
                            text: "连接边缘端"
                            tone: "primary"
                            onClicked: if (root.viewModel) root.viewModel.connectEdge()
                        }

                        AppButton {
                            text: "断开连接"
                            tone: "secondary"
                            onClicked: if (root.viewModel) root.viewModel.disconnectEdge()
                        }

                        AppButton {
                            text: "PING"
                            tone: "secondary"
                            onClicked: if (root.viewModel) root.viewModel.sendPing()
                        }

                        AppButton {
                            text: "加载样本"
                            tone: "secondary"
                            onClicked: if (root.viewModel) root.viewModel.loadSamples()
                        }

                        AppComboBox {
                            id: sampleBox
                            Layout.fillWidth: true
                            model: root.viewModel ? root.viewModel.sampleNames : []
                            currentIndex: root.viewModel ? root.viewModel.selectedSampleIndex : -1
                            onActivated: if (root.viewModel) root.viewModel.selectedSampleIndex = currentIndex
                        }

                        AppButton {
                            text: "播放样本"
                            tone: "success"
                            onClicked: if (root.viewModel) root.viewModel.playSelectedSample()
                        }
                    }

                    AppTextArea {
                        id: manualInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 118
                        placeholderText: "可在此粘贴 187 维特征，或点击“生成演示输入”后发送推理。"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        AppButton {
                            text: "生成演示输入"
                            tone: "secondary"
                            onClicked: if (root.viewModel) manualInput.text = root.viewModel.generateDemoInput()
                        }
                        AppButton {
                            text: "发送推理"
                            tone: "primary"
                            onClicked: if (root.viewModel) root.viewModel.sendPredict(manualInput.text)
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredWidth: root.width * 0.3
                Layout.preferredHeight: 660
                title: "推理结果"
                subtitle: "模型状态、置信度与推理延时"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 18
                        color: Theme.levelBackground(root.viewModel ? root.viewModel.alertLevel : "normal")
                        border.width: 1
                        border.color: Theme.levelColor(root.viewModel ? root.viewModel.alertLevel : "normal")
                        implicitHeight: 132

                        Behavior on color {
                            ColorAnimation { duration: 180 }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 8

                            Label {
                                text: root.viewModel && root.viewModel.predLabel.length > 0 ? root.viewModel.predLabel : "等待推理结果"
                                color: Theme.levelColor(root.viewModel ? root.viewModel.alertLevel : "normal")
                                font.pixelSize: 26
                                font.bold: true
                            }
                            Label {
                                text: root.viewModel && root.viewModel.alertLevel === "critical" ? "严重异常，请及时处理" :
                                      root.viewModel && root.viewModel.alertLevel === "warning" ? "存在异常趋势，建议复核" : "当前节律稳定"
                                color: Theme.textSecondary
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: "置信度"; color: Theme.textSecondary }
                        ProgressBar {
                            id: confidenceBar
                            Layout.fillWidth: true
                            value: root.viewModel ? root.viewModel.confidencePercent / 100.0 : 0
                            background: Rectangle {
                                implicitHeight: 10
                                radius: 5
                                color: Theme.surfaceAlt
                            }
                            contentItem: Item {
                                implicitHeight: 10
                                Rectangle {
                                    width: parent.width * confidenceBar.visualPosition
                                    height: parent.height
                                    radius: 5
                                    color: Theme.primary
                                }
                            }
                        }
                        Label {
                            text: root.viewModel && root.viewModel.confidenceText.length > 0 ? root.viewModel.confidenceText : "0%"
                            color: Theme.text
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            implicitHeight: 96
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                Label { text: "告警级别"; color: Theme.textMuted; font.pixelSize: 12 }
                                StatusPill {
                                    text: root.viewModel ? root.viewModel.alertLevel : "normal"
                                    level: root.viewModel ? root.viewModel.alertLevel : "normal"
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            implicitHeight: 96
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                Label { text: "推理延时"; color: Theme.textMuted; font.pixelSize: 12 }
                                Label {
                                    text: root.viewModel && root.viewModel.latencyText.length > 0 ? root.viewModel.latencyText : "--"
                                    color: Theme.text
                                    font.pixelSize: 24
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Rectangle {
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
                            Label {
                                text: "说明"
                                color: Theme.text
                                font.bold: true
                            }
                            Label {
                                text: "这里接的是边缘推理链路。后续如果接真实传感器，数据采集端只需要把波形片段发送给香橙派即可。"
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            title: "历史片段"
            subtitle: "最近一次次推理结果会在这里横向保存，点击即可回放波形。"

            ListView {
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: 14
                clip: true
                model: root.viewModel ? root.viewModel.historyModel : null

                delegate: Rectangle {
                    id: historyCard
                    required property int index
                    required property string time
                    required property string predLabel
                    required property string alertLevel
                    required property string confidence
                    required property string source
                    width: 240
                    height: 150
                    radius: 18
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border
                    property bool hovered: false

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: parent.hovered = true
                        onExited: parent.hovered = false
                        onClicked: if (root.viewModel) root.viewModel.replayHistory(historyCard.index)
                    }

                    Behavior on y {
                        NumberAnimation { duration: 120 }
                    }

                    y: hovered ? -4 : 0

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8
                        Label { text: historyCard.time; color: Theme.textMuted; font.pixelSize: 12 }
                        Label { text: historyCard.predLabel; color: Theme.text; font.pixelSize: 18; font.bold: true }
                        StatusPill { text: historyCard.alertLevel; level: historyCard.alertLevel }
                        Label { text: "置信度：" + historyCard.confidence; color: Theme.textSecondary }
                        Label { text: "来源：" + historyCard.source; color: Theme.textMuted; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}



