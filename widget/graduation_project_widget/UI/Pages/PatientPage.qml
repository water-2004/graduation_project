import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import "../Components"
import graduation_project.widget

Item {
    id: root
    property var appViewModel
    property int editingRow: -1
    property int pendingDeleteRow: -1
    property string pendingDeleteName: ""

    function openCreate() {
        editingRow = -1
        patientIdField.text = ""
        nameField.text = ""
        genderBox.currentIndex = 0
        ageField.text = ""
        phoneField.text = ""
        remarkField.text = ""
        editorDrawer.open()
    }

    function openEdit(row) {
        editingRow = row
        var data = appViewModel.patientFormData(row)
        patientIdField.text = data.patientId || ""
        nameField.text = data.name || ""
        var genderIndex = ["男", "女", "未知"].indexOf(data.gender || "男")
        genderBox.currentIndex = genderIndex >= 0 ? genderIndex : 0
        ageField.text = data.age !== undefined ? String(data.age) : ""
        phoneField.text = data.phone || ""
        remarkField.text = data.remark || ""
        editorDrawer.open()
    }

    function askDelete(row, name) {
        pendingDeleteRow = row
        pendingDeleteName = name || "该病人"
        deleteDialog.open()
    }

    ConfirmDialog {
        id: deleteDialog
        anchors.centerIn: Overlay.overlay
        titleText: "确认删除病人"
        messageText: "删除后该病人的基础档案将被移除。\n当前对象：" + pendingDeleteName + "\n请确认是否继续。"
        confirmText: "确认删除"
        cancelText: "取消"
        tone: "danger"
        onConfirmed: {
            if (pendingDeleteRow >= 0) {
                root.appViewModel.deletePatient(pendingDeleteRow)
            }
            pendingDeleteRow = -1
            pendingDeleteName = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            AppTextField {
                Layout.fillWidth: true
                placeholderText: "按病人编号、姓名、性别、电话或备注搜索"
                onTextChanged: root.appViewModel.setPatientFilter(text)
            }

            AppButton {
                text: "刷新"
                tone: "secondary"
                onClicked: root.appViewModel.refreshAll()
            }

            AppButton {
                text: "新增病人"
                tone: "primary"
                onClicked: root.openCreate()
            }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "病人列表"
            subtitle: "采用表格 + 抽屉模式，主区专注浏览，新增/编辑在右侧抽屉完成。"

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 46
                    radius: 14
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12
                        Label { text: "编号"; Layout.preferredWidth: 110; color: Theme.textMuted; font.bold: true }
                        Label { text: "姓名"; Layout.preferredWidth: 90; color: Theme.textMuted; font.bold: true }
                        Label { text: "性别/年龄"; Layout.preferredWidth: 120; color: Theme.textMuted; font.bold: true }
                        Label { text: "电话"; Layout.fillWidth: true; color: Theme.textMuted; font.bold: true }
                        Label { text: "备注"; Layout.fillWidth: true; color: Theme.textMuted; font.bold: true }
                        Label { text: "操作"; Layout.preferredWidth: 250; color: Theme.textMuted; font.bold: true }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    clip: true
                    model: root.appViewModel.patientModel

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 88
                        radius: 18
                        color: hovered ? "#F8FAFC" : Theme.surface
                        border.width: 1
                        border.color: hovered ? "#CBD5E1" : Theme.border

                        property bool hovered: false

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Label { text: patientId; Layout.preferredWidth: 110; color: Theme.text; font.bold: true }
                            Label { text: name; Layout.preferredWidth: 90; color: Theme.text }
                            Label { text: gender + " / " + age + "岁"; Layout.preferredWidth: 120; color: Theme.textSecondary }
                            Label { text: phone; Layout.fillWidth: true; color: Theme.textSecondary; elide: Text.ElideRight }
                            Label { text: remark; Layout.fillWidth: true; color: Theme.textMuted; elide: Text.ElideRight }

                            RowLayout {
                                Layout.preferredWidth: 250
                                spacing: 8

                                AppButton {
                                    compact: true
                                    text: "编辑"
                                    tone: "secondary"
                                    onClicked: root.openEdit(index)
                                }

                                AppButton {
                                    compact: true
                                    text: "删除"
                                    tone: "danger"
                                    onClicked: root.askDelete(index, name)
                                }

                                AppButton {
                                    compact: true
                                    text: "进入监测"
                                    tone: "success"
                                    onClicked: root.appViewModel.enterMonitorWithPatient(index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Drawer {
        id: editorDrawer
        width: Math.min(root.width * 0.38, 430)
        height: root.height
        edge: Qt.RightEdge
        modal: true
        interactive: true

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "position"; from: 1.0; to: 0.0; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
            }
        }

        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "position"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.InCubic }
                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 120 }
            }
        }

        background: Rectangle {
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
            radius: 22
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Label {
                text: root.editingRow >= 0 ? "编辑病人" : "新增病人"
                color: Theme.text
                font.pixelSize: 24
                font.bold: true
            }

            Label {
                text: "右侧抽屉只负责输入与保存，不打断主列表浏览。"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            AppTextField {
                id: patientIdField
                Layout.fillWidth: true
                placeholderText: "病人编号，如 P001"
            }

            AppTextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "姓名"
            }

            AppComboBox {
                id: genderBox
                Layout.fillWidth: true
                model: ["男", "女", "未知"]
            }

            AppTextField {
                id: ageField
                Layout.fillWidth: true
                placeholderText: "年龄"
                validator: IntValidator { bottom: 0; top: 150 }
            }

            AppTextField {
                id: phoneField
                Layout.fillWidth: true
                placeholderText: "电话"
            }

            AppTextArea {
                id: remarkField
                Layout.fillWidth: true
                Layout.fillHeight: true
                placeholderText: "备注"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                AppButton {
                    Layout.fillWidth: true
                    text: "取消"
                    tone: "secondary"
                    onClicked: editorDrawer.close()
                }

                AppButton {
                    Layout.fillWidth: true
                    text: "保存"
                    tone: "primary"
                    onClicked: {
                        root.appViewModel.savePatient(
                                    root.editingRow,
                                    patientIdField.text,
                                    nameField.text,
                                    genderBox.currentText,
                                    Number(ageField.text),
                                    phoneField.text,
                                    remarkField.text)
                        editorDrawer.close()
                    }
                }
            }
        }
    }
}

