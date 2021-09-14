/**
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "omxUtils.h"

sp<IMediaPlayerService> mediaPlayerService = NULL;
sp<IOMXNode> mOMXNode = 0;
sp<IOMX> mOMX;
omx_message msg;
Mutex mLock;
Condition mMessageAddedCondition;
int32_t mLastMsgGeneration;
int32_t mCurGeneration;
List<omx_message> mMessageQueue;

struct CodecObserver : public BnOMXObserver {
 public:
    CodecObserver(int32_t gen)
            : mGeneration(gen) {
    }

    void onMessages(const std::list<omx_message> &messages) override;
    int32_t mGeneration;

 protected:
    virtual ~CodecObserver() {
    }
};
void handleMessages(int32_t gen, const std::list<omx_message> &messages) {
    Mutex::Autolock autoLock(mLock);
    for (std::list<omx_message>::const_iterator it = messages.cbegin();
            it != messages.cend();) {
        mMessageQueue.push_back(*it++);
        mLastMsgGeneration = gen;
    }
    mMessageAddedCondition.signal();
}
void CodecObserver::onMessages(const std::list<omx_message> &messages) {
    handleMessages(mGeneration, messages);
}

status_t dequeueMessageForNode(omx_message *msg, int64_t timeoutUs) {
    int64_t finishBy = ALooper::GetNowUs() + timeoutUs;
    status_t err = OK;

    while (err != TIMED_OUT) {
        Mutex::Autolock autoLock(mLock);
        if (mLastMsgGeneration < mCurGeneration) {
            mMessageQueue.clear();
        }
        // Messages are queued in batches, if the last batch queued is
        // from a node that already expired, discard those messages.
        List<omx_message>::iterator it = mMessageQueue.begin();
        while (it != mMessageQueue.end()) {
            *msg = *it;
            mMessageQueue.erase(it);
            return OK;
        }
        if (timeoutUs < 0) {
            err = mMessageAddedCondition.wait(mLock);
        } else {
            err = mMessageAddedCondition.waitRelative(
                    mLock, (finishBy - ALooper::GetNowUs()) * 1000);
        }
    }
    return err;
}
void omxUtilsCheckCmdExecution(char *name) {
    status_t err = dequeueMessageForNode(&msg, DEFAULT_TIMEOUT);
    if (err == TIMED_OUT) {
        ALOGE("[omxUtils] OMX command timed out for %s, exiting the app", name);
        exit (EXIT_FAILURE);
    }
}
void omxExitOnError(status_t ret) {
    if (ret != OK) {
        exit (EXIT_FAILURE);
    }
}
status_t omxUtilsInit(char *codecName) {
    android::ProcessState::self()->startThreadPool();
    using namespace ::android::hardware::media::omx::V1_0;
    sp<IOmx> tOmx = IOmx::getService();
    if (tOmx == nullptr) {
        return NO_INIT;
    }
    mOMX = new utils::LWOmx(tOmx);
    sp<CodecObserver> observer = new CodecObserver(++mCurGeneration);
    return mOMX->allocateNode(codecName, observer, &mOMXNode);
}
status_t omxUtilsGetParameter(int portIndex,
                              OMX_PARAM_PORTDEFINITIONTYPE *params) {
    InitOMXParams(params);
    params->nPortIndex = portIndex;
    return mOMXNode->getParameter(OMX_IndexParamPortDefinition, params,
                                  sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
}
status_t omxUtilsSetParameter(int portIndex,
                              OMX_PARAM_PORTDEFINITIONTYPE *params) {
    InitOMXParams(params);
    params->nPortIndex = portIndex;
    return mOMXNode->setParameter(OMX_IndexParamPortDefinition, params,
                                  sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
}
status_t omxUtilsSetPortMode(OMX_U32 portIndex, IOMX::PortMode mode) {
    return mOMXNode->setPortMode(portIndex, mode);
}
status_t omxUtilsUseBuffer(OMX_U32 portIndex, const OMXBuffer &omxBuf,
                           android::BnOMX::buffer_id *buffer) {
    return mOMXNode->useBuffer(portIndex, omxBuf, buffer);
}
status_t omxUtilsSendCommand(OMX_COMMANDTYPE cmd, OMX_S32 param) {
    int ret = mOMXNode->sendCommand(cmd, param);
    omxUtilsCheckCmdExecution((char *) __FUNCTION__);
    return ret;
}
status_t omxUtilsEmptyBuffer(android::BnOMX::buffer_id buffer,
                             const OMXBuffer &omxBuf, OMX_U32 flags,
                             OMX_TICKS timestamp, int fenceFd) {
    return mOMXNode->emptyBuffer(buffer, omxBuf, flags, timestamp, fenceFd);
}
status_t omxUtilsFillBuffer(android::BnOMX::buffer_id buffer,
                            const OMXBuffer &omxBuf, int fenceFd) {
    return mOMXNode->fillBuffer(buffer, omxBuf, fenceFd);
}
status_t omxUtilsFreeBuffer(OMX_U32 portIndex,
                            android::BnOMX::buffer_id buffer) {
    return mOMXNode->freeBuffer(portIndex, buffer);
}
status_t omxUtilsFreeNode() {
    return mOMXNode->freeNode();
}
