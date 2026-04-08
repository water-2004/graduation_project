import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import "../Components"

Item {
    id: root
    signal loginRequested(string host, int port, string username, string password)

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#E0F2FE" }
            GradientStop { position: 0.35; color: Theme.background }
            GradientStop { position: 1.0; color: "#F8FAFC" }
        }
    }

    Rectangle {
        width: 360
        height: 360
        radius: 180
        color: Qt.rgba(0.0, 0.43, 0.85, 0.08)
        x: -80
        y: -60
    }

    Rectangle {
        width: 280
        height: 280
        radius: 140
        color: Qt.rgba(0.05, 0.58, 0.53, 0.08)
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 120
        anchors.topMargin: 80
    }

    Rectangle {
        id: loginCard
        width: 540
        height: 660
        anchors.centerIn: parent
        radius: 32
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        opacity: 0.0
        scale: 0.985

        transform: Translate {
            id: cardTranslate
            y: 20
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 34
            spacing: 18

            Rectangle {
                width: 84
                height: 84
                radius: 26
                color: Theme.primary
                Layout.alignment: Qt.AlignHCenter

                Label {
                    anchors.centerIn: parent
                    text: "ECG"
                    color: "white"
                    font.pixelSize: 30
                    font.bold: true
                }
            }

            Label {
                text: "智能心电边缘监测平台"
                Layout.alignment: Qt.AlignHCenter
                color: Theme.text
                font.pixelSize: 28
                font.bold: true
            }

            Label {
                text: "登录后直接进入主界面，查看实时监测、病人管理、报警中心与系统概览。"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.textSecondary
                font.pixelSize: 14
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 18
                color: Theme.primarySoft
                border.width: 1
                border.color: "#BFDBFE"
                implicitHeight: 96

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6
                    Label {
                        text: "默认测试账号"
                        color: Theme.primary
                        font.bold: true
                    }
                    Label {
                        text: "业务服务默认：127.0.0.1:9200\n账号：admin  密码：123456"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: 14
                rowSpacing: 14
                Layout.fillWidth: true

                Label { text: "业务主机"; color: Theme.textSecondary }
                AppTextField {
                    id: hostField
                    text: "127.0.0.1"
                    Layout.fillWidth: true
                    placeholderText: "输入业务服务地址"
                }

                Label { text: "业务端口"; color: Theme.textSecondary }
                AppTextField {
                    id: portField
                    text: "9200"
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 1; top: 65535 }
                    placeholderText: "输入业务端口"
                }

                Label { text: "用户名"; color: Theme.textSecondary }
                AppTextField {
                    id: usernameField
                    text: "admin"
                    Layout.fillWidth: true
                }

                Label { text: "密码"; color: Theme.textSecondary }
                AppTextField {
                    id: passwordField
                    text: "123456"
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    onAccepted: root.loginRequested(hostField.text, Number(portField.text), usernameField.text, passwordField.text)
                }
            }

            Item { Layout.fillHeight: true }

            AppButton {
                Layout.fillWidth: true
                implicitHeight: 52
                tone: "primary"
                text: "登录并进入系统"
                onClicked: root.loginRequested(hostField.text,
                                               Number(portField.text),
                                               usernameField.text,
                                               passwordField.text)
            }
        }
    }

    ParallelAnimation {
        id: introAnimation
        NumberAnimation {
            target: loginCard
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 220
        }
        NumberAnimation {
            target: loginCard
            property: "scale"
            from: 0.985
            to: 1.0
            duration: 220
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: cardTranslate
            property: "y"
            from: 20
            to: 0
            duration: 260
            easing.type: Easing.OutCubic
        }
    }

    Component.onCompleted: introAnimation.start()
}
