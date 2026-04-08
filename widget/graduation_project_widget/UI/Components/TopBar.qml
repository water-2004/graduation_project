import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme
import graduation_project.widget

Rectangle {
    id: root
    color: Theme.surface
    radius: 18
    border.width: 1
    border.color: Theme.border
    implicitHeight: 84

    property string title: ""
    property string subtitle: ""
    property string businessState: "disconnected"
    property string businessStateText: "业务服务未连接"
    property string userName: "未登录"
    property string userRole: ""
    property int notificationCount: 0
    signal logoutRequested()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: root.title
                color: Theme.text
                font.pixelSize: 24
                font.bold: true
            }
            Label {
                text: root.subtitle
                color: Theme.textSecondary
                font.pixelSize: 13
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        StatusPill {
            text: root.businessStateText
            level: root.businessState
        }

        Rectangle {
            width: 42
            height: 42
            radius: 21
            color: Theme.primarySoft
            border.color: Theme.border

            Label {
                anchors.centerIn: parent
                text: root.notificationCount > 99 ? "99+" : String(root.notificationCount)
                color: Theme.primary
                font.bold: true
            }
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            color: Theme.border
        }

        RowLayout {
            spacing: 10

            Rectangle {
                width: 42
                height: 42
                radius: 21
                color: Theme.primary
                Label {
                    anchors.centerIn: parent
                    text: root.userName.length > 0 ? root.userName.charAt(0) : "U"
                    color: "white"
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                Label {
                    text: root.userName
                    color: Theme.text
                    font.pixelSize: 14
                    font.bold: true
                }
                Label {
                    text: root.userRole.length > 0 ? root.userRole : "管理员"
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }
        }
    }
}
