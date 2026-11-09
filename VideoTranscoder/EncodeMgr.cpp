//
// Created by mpt on 2026/11/2.
//

#include "EncodeMgr.h"

#include "EncodeWorker.h"
#include "const.h"

EncodeMgr::EncodeMgr() {
    _worker = std::make_unique<EncodeWorker>();
}

bool EncodeMgr::PostEncodeQueue(std::unique_ptr<RequestInfo> request_info) {
    if (!_worker) {
        return false;
    }
    return _worker->postEncodeQueue(std::move(request_info));
}

std::size_t EncodeMgr::FreeSlot() {
    if (!_worker) {
        return 0;
    }
    return _worker->FreeSlot();
}

void EncodeMgr::Stop() {
    if (_worker) {
        _worker->Stop();
    }
}

EncodeMgr::~EncodeMgr() {
    Stop();
}