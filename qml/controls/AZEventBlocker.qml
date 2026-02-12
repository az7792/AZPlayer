// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

MouseArea {
    id: blocker
    hoverEnabled: true
    acceptedButtons: Qt.AllButtons
    onWheel: (wheel) => wheel.accepted = true
}
