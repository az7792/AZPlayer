// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODESUBTITLE_H
#define DECODESUBTITLE_H
#include "decode/decodebase.h"
class DecodeSubtitle : public DecodeBase {
    Q_OBJECT
public:
    using DecodeBase::DecodeBase;
    [[nodiscard]] bool init(AVStream *stream,
                            sharedPktQueue pktBuf,
                            sharedFrmQueue frmBuf,
                            int threadNum);

private:
    void decodingLoop() override;
};

#endif // DECODESUBTITLE_H
