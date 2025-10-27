// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODESUBTITLE_H
#define DECODESUBTITLE_H
#include "decodebase.h"
class DecodeSubtitle : public DecodeBase {
public:
    using DecodeBase::DecodeBase;
    bool init(AVStream *stream,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

private:
    void decodingLoop() override;
};

#endif // DECODESUBTITLE_H
