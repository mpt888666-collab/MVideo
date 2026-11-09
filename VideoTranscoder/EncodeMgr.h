//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_ENCODEMGR_H
#define VIDEOTRANSCODER_ENCODEMGR_H
#include <cstddef>
#include <memory>
#include <queue>

#include "Singleton.h"

struct RequestInfo;
class EncodeWorker;

class EncodeMgr : public Singleton<EncodeMgr> {
    friend class Singleton<EncodeMgr>;

public:
    bool PostEncodeQueue(std::unique_ptr<RequestInfo> request_info);

    std::size_t FreeSlot();

    void Stop();
    ~EncodeMgr();

private:
    EncodeMgr();
    std::unique_ptr<EncodeWorker> _worker;
};

#endif  // VIDEOTRANSCODER_ENCODEMGR_H