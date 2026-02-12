// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODEAUDIO_H
#define DECODEAUDIO_H
#include "decodebase.h"

class DecodeAudio : public DecodeBase {
    Q_OBJECT
public:
    using DecodeBase::DecodeBase;
    bool init(AVStream *stream,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

private:
    void decodingLoop() override;
    AudioPar m_oldPar;
};

#endif // DECODEAUDIO_H
