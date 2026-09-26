/* SPDX-License-Identifier: LGPL-2.0-or-later */

import QtQuick
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

InlineMessage {
    id: root
    // The address that deletes this upload from the server. Empty if the server has no delete support.
    property string deleteUrl: ""
    type: Kirigami.MessageType.Information
    text: i18n("The link has been copied to the clipboard.")
    // Not using showCloseButton because it toggles visible on this item,
    // making it harder to use with loaders.
    actions: [
        Kirigami.Action {
            visible: root.deleteUrl.length > 0
            icon.name: "edit-delete"
            text: i18n("Delete")
            onTriggered: {
                SpectacleCore.deleteUpload(root.deleteUrl)
                root.loader.state = "inactive"
            }
        },
        Kirigami.Action {
            displayComponent: QQC.ToolButton {
                icon.name: "dialog-close"
                onClicked: root.loader.state = "inactive"
            }
        }
    ]
    Timer {
        running: true
        // Leave more time when there is a Delete button to reach.
        interval: root.deleteUrl.length > 0 ? 30000 : 10000
        onTriggered: root.loader.state = "inactive"
    }
}
