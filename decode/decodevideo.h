#ifndef DECODEVIDEO_H
#define DECODEVIDEO_H
#include "decodebase.h"
class DecodeVideo : public DecodeBase {
    Q_OBJECT
public:
    using DecodeBase::DecodeBase;
    bool init(AVStream *stream,
              sharedPktQueue pktBuf,
              sharedFrmQueue frmBuf);

private:
    void decodingLoop() override;
};

#endif // DECODEVIDEO_H
