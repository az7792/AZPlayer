#ifndef DECODEVIDEO_H
#define DECODEVIDEO_H
#include "decodebase.h"
#include "utils.h"
class DecodeVideo : public DecodeBase {
    Q_OBJECT
public:
    DecodeVideo();

    bool init(enum AVCodecID id,
              const struct AVCodecParameters *par,
              AVRational time_base,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);
public slots:
    void startDecoding() override;
};

#endif // DECODEVIDEO_H
