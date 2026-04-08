import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "Theme.js" as Theme
import graduation_project.widget

ApplicationWindow {
    id: window
    width: 1560
    height: 980
    visible: true
    title: "智能心电边缘监测平台"
    color: Theme.background

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    Rectangle {
        width: 520
        height: 520
        radius: 260
        color: Qt.rgba(0.0, 0.43, 0.85, 0.05)
        x: width * -0.18
        y: height * 0.08
    }

    Rectangle {
        width: 380
        height: 380
        radius: 190
        color: Qt.rgba(0.05, 0.58, 0.53, 0.06)
        x: width * 0.82
        y: height * 0.62
    }

    property string criticalAlertMessage: ""

    Toast {
        id: toast
        anchors.top: parent.top
        anchors.topMargin: 28
    }

    SoundEffect {
        id: criticalAlertSound
        source: Qt.resolvedUrl("audio/critical_alert.wav")
        volume: 0.9
        loopCount: 2
    }

    Popup {
        id: criticalAlertPopup
        anchors.centerIn: Overlay.overlay
        width: 520
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.NoAutoClose

        background: Rectangle {
            radius: 26
            color: Theme.surface
            border.width: 2
            border.color: Theme.critical
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 16
                    color: Theme.criticalSoft
                    border.width: 1
                    border.color: Theme.critical

                    Label {
                        anchors.centerIn: parent
                        text: "!"
                        color: Theme.critical
                        font.pixelSize: 28
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: "严重报警"
                        color: Theme.text
                        font.pixelSize: 26
                        font.bold: true
                    }
                    Label {
                        text: "系统检测到高风险心电异常，请立即复核并处理。"
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 18
                color: Theme.criticalSoft
                border.width: 1
                border.color: Theme.critical
                implicitHeight: detailLabel.implicitHeight + 28

                Label {
                    id: detailLabel
                    anchors.fill: parent
                    anchors.margins: 14
                    text: window.criticalAlertMessage
                    color: Theme.text
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Label {
                text: "建议动作：1. 查看报警中心  2. 核对病人波形  3. 联系值班人员。"
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    text: "查看报警中心"
                    onClicked: {
                        criticalAlertPopup.close()
                        appViewModel.navigate(3)
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "知道了"
                    onClicked: criticalAlertPopup.close()
                }
            }
        }
    }

    Connections {
        target: appViewModel
        function onToastRequested(message, level) {
            toast.showToast(message, level)
            if (level === "critical" && message.indexOf("?????") === 0) {
                window.criticalAlertMessage = message
                criticalAlertPopup.close()
                criticalAlertPopup.open()
                criticalAlertSound.stop()
                criticalAlertSound.play()
            }
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: appViewModel.loggedIn ? shellComponent : loginComponent
    }

    Component {
        id: loginComponent
        LoginPage {
            anchors.fill: parent
            onLoginRequested: appViewModel.login(host, port, username, password)
        }
    }

    Component {
        id: shellComponent
        Item {
            anchors.fill: parent

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Sidebar {
                    Layout.fillHeight: true
                    currentPage: appViewModel.currentPage
                    onNavigate: appViewModel.navigate(page)
                    onLogoutRequested: appViewModel.logout()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 18

                    TopBar {
                        Layout.fillWidth: true
                        Layout.topMargin: 18
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        title: appViewModel.pageTitle
                        subtitle: appViewModel.pageSubtitle
                        businessState: appViewModel.businessState
                        businessStateText: appViewModel.businessStateText
                        userName: appViewModel.currentUserName
                        userRole: appViewModel.currentUserRole
                        notificationCount: appViewModel.notificationCount
                    }

                    Item {
                        id: pageFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        Layout.bottomMargin: 18
                        opacity: 1.0

                        transform: Translate {
                            id: pageTranslate
                            x: 0
                        }

                        StackLayout {
                            anchors.fill: parent
                            currentIndex: appViewModel.currentPage

                            DashboardPage {
                                viewModel: appViewModel.dashboardViewModel
                            }
                            PatientPage {
                                appViewModel: appViewModel
                            }
                            MonitorPage {
                                viewModel: appViewModel.monitorViewModel
                            }
                            AlertCenterPage {
                                appViewModel: appViewModel
                            }
                            AboutPage {
                                appViewModel: appViewModel
                            }
                        }
                    }
                }
            }

            Connections {
                target: appViewModel
                function onCurrentPageChanged() {
                    pageSwitchAnimation.restart()
                }
            }

            ParallelAnimation {
                id: pageSwitchAnimation
                NumberAnimation {
                    target: pageFrame
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: 180
                }
                NumberAnimation {
                    target: pageTranslate
                    property: "x"
                    from: 16
                    to: 0
                    duration: 220
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
