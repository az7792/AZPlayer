#ifndef DECODEAUDIO_H
#define DECODEAUDIO_H
#include "decodebase.h"
#include "utils.h"
#include <QAudioDevice>

class DecodeAudio : public DecodeBase {
    Q_OBJECT
public:
    DecodeAudio();

    bool init(enum AVCodecID id,
              const struct AVCodecParameters *par,
              AVRational time_base,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

public slots:
    void startDecoding() override;

private:
    AudioPar m_oldPar;
};

#endif // DECODEAUDIO_H
