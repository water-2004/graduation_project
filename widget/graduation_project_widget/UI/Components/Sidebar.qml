import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Rectangle {
    id: root
    width: 236
    color: Theme.sidebarBg

    property int currentPage: 0
    signal navigate(int page)
    signal logoutRequested()

    readonly property var navItems: [
        { title: "系统概览", icon: "◫", page: 0 },
        { title: "病人管理", icon: "⌘", page: 1 },
        { title: "实时监测", icon: "≈", page: 2 },
        { title: "报警中心", icon: "!", page: 3 },
        { title: "关于系统", icon: "i", page: 4 }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true

            Label {
                text: "ECG Edge Monitor"
                color: "white"
                font.pixelSize: 24
                font.bold: true
            }

            Label {
                text: "智能心电边缘监测平台"
                color: Theme.sidebarText
                font.pixelSize: 13
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#334155"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.navItems

                delegate: Button {
                    id: navButton
                    Layout.fillWidth: true
                    implicitHeight: 50
                    checkable: true
                    checked: root.currentPage === modelData.page
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 12
                        color: navButton.checked ? Theme.sidebarActive : navButton.hovered ? Theme.sidebarHover : "transparent"
                        border.width: navButton.checked ? 1 : 0
                        border.color: navButton.checked ? Theme.teal : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 12
                        Label {
                            text: modelData.icon
                            color: navButton.checked ? Theme.success : Theme.sidebarText
                            font.pixelSize: 18
                            Layout.leftMargin: 12
                        }
                        Label {
                            text: modelData.title
                            color: navButton.checked ? "white" : Theme.sidebarText
                            font.pixelSize: 15
                            font.bold: navButton.checked
                            Layout.fillWidth: true
                        }
                    }

                    onClicked: root.navigate(modelData.page)
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 14
            color: "#0F172A"
            border.color: "#334155"
            border.width: 1
            implicitHeight: 108

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Label {
                    text: "设计方向"
                    color: "white"
                    font.bold: true
                }
                Label {
                    text: "QML + C++ Backend\n低耦合、可扩展、便于后续接入更多端。"
                    color: Theme.sidebarText
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }
            }
        }

        Button {
            id: logoutButton
            Layout.fillWidth: true
            implicitHeight: 46
            hoverEnabled: true
            background: Rectangle {
                radius: 12
                color: logoutButton.hovered ? "#7F1D1D" : "#991B1B"
                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
            }
            contentItem: Label {
                text: "退出登录"
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
            }
            onClicked: root.logoutRequested()
        }
    }
}
