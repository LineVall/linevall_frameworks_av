/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_STREAM_OUT_SINK_H
#define ANDROID_AUDIO_STREAM_OUT_SINK_H

#include <audio_utils/MelProcessor.h>
#include <media/nbaio/NBAIO.h>
#include <mediautils/Synchronization.h>

namespace android {

class StreamOutHalInterface;

// not multi-thread safe
class AudioStreamOutSink : public NBAIO_Sink {

public:
    AudioStreamOutSink(sp<StreamOutHalInterface> stream);
    virtual ~AudioStreamOutSink();

    // NBAIO_Port interface

    virtual ssize_t negotiate(const NBAIO_Format offers[], size_t numOffers,
                              NBAIO_Format counterOffers[], size_t& numCounterOffers);
    //virtual NBAIO_Format format();

    // NBAIO_Sink interface

    //virtual size_t framesWritten() const;
    //virtual size_t framesUnderrun() const;
    //virtual size_t underruns() const;

    virtual ssize_t write(const void *buffer, size_t count);

    virtual status_t getTimestamp(ExtendedTimestamp &timestamp);

    // NBAIO_Sink end

    void startMelComputation(const sp<audio_utils::MelProcessor>& processor);

    void stopMelComputation();

#if 0   // until necessary
    sp<StreamOutHalInterface> stream() const { return mStream; }
#endif

private:
    sp<StreamOutHalInterface> mStream;
    size_t              mStreamBufferSizeBytes; // as reported by get_buffer_size()
    mediautils::atomic_sp<audio_utils::MelProcessor> mMelProcessor;
};

}   // namespace android

#endif  // ANDROID_AUDIO_STREAM_OUT_SINK_H
