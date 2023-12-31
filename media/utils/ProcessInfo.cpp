/*
 * Copyright (C) 2015 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ProcessInfo"
#include <utils/Log.h>

#include <mediautils/ProcessInfo.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <private/android_filesystem_config.h>
#include <processinfo/IProcessInfoService.h>

namespace android {

static constexpr int32_t INVALID_ADJ = -10000;
static constexpr int32_t NATIVE_ADJ = -1000;

/* Make sure this matches with ActivityManager::PROCESS_STATE_NONEXISTENT
 * #include <binder/ActivityManager.h>
 * using ActivityManager::PROCESS_STATE_NONEXISTENT;
 */
static constexpr int32_t PROCESS_STATE_NONEXISTENT = 20;

ProcessInfo::ProcessInfo() {}

/*
 * Checks whether the list of processes with given pids exist or not.
 *
 * Arguments:
 *  - pids (input): List of pids for which to check whether they are Existent or not.
 *  - existent (output): boolean vector corresponds to Existent state of each pids.
 *
 * On successful return:
 *     - existent[i] true corresponds to pids[i] still active and
 *     - existent[i] false corresponds to pids[i] already terminated (Nonexistent)
 * On unsuccessful return, the output argument existent is invalid.
 */
bool ProcessInfo::checkProcessExistent(const std::vector<int32_t>& pids,
                                       std::vector<bool>* existent) {
    sp<IBinder> binder = defaultServiceManager()->waitForService(String16("processinfo"));
    sp<IProcessInfoService> service = interface_cast<IProcessInfoService>(binder);

    // Get the process state of the applications managed/tracked by the ActivityManagerService.
    // Don't have to look into the native processes.
    // If we really need the state of native process, then we can use ==> mOverrideMap
    size_t count = pids.size();
    std::vector<int32_t> states(count, PROCESS_STATE_NONEXISTENT);
    status_t err = service->getProcessStatesFromPids(count,
                                                     const_cast<int32_t*>(pids.data()),
                                                     states.data());
    if (err != OK) {
        ALOGE("%s: IProcessInfoService::getProcessStatesFromPids failed with %d",
              __func__, err);
        return false;
    }

    existent->clear();
    for (size_t index = 0; index < states.size(); index++) {
        // If this process is not tracked by ActivityManagerService, look for overrides.
        if (states[index] == PROCESS_STATE_NONEXISTENT) {
            std::scoped_lock lock{mOverrideLock};
            std::map<int, ProcessInfoOverride>::iterator it =
                mOverrideMap.find(pids[index]);
            if (it != mOverrideMap.end()) {
                states[index] = it->second.procState;
            }
        }
        existent->push_back(states[index] != PROCESS_STATE_NONEXISTENT);
    }

    return true;
}

bool ProcessInfo::getPriority(int pid, int* priority) {
    sp<IBinder> binder = defaultServiceManager()->waitForService(String16("processinfo"));
    sp<IProcessInfoService> service = interface_cast<IProcessInfoService>(binder);

    size_t length = 1;
    int32_t state;
    int32_t score = INVALID_ADJ;
    status_t err = service->getProcessStatesAndOomScoresFromPids(length, &pid, &state, &score);
    ALOGV("%s: pid:%d state:%d score:%d err:%d", __FUNCTION__, pid, state, score, err);
    if (err != OK) {
        ALOGE("getProcessStatesAndOomScoresFromPids failed");
        return false;
    }
    if (score <= NATIVE_ADJ) {
        std::scoped_lock lock{mOverrideLock};

        // If this process if not tracked by ActivityManagerService, look for overrides.
        auto it = mOverrideMap.find(pid);
        if (it != mOverrideMap.end()) {
            ALOGI("pid %d invalid OOM score %d, override to %d", pid, score, it->second.oomScore);
            score = it->second.oomScore;
        } else {
            ALOGE("pid %d invalid OOM score %d", pid, score);
            return false;
        }
    }

    // Use OOM adjustments value as the priority. Lower the value, higher the priority.
    *priority = score;
    return true;
}

bool ProcessInfo::isPidTrusted(int pid) {
    return isPidUidTrusted(pid, -1);
}

bool ProcessInfo::isPidUidTrusted(int pid, int uid) {
    int callingPid = IPCThreadState::self()->getCallingPid();
    int callingUid = IPCThreadState::self()->getCallingUid();
    // Always trust when the caller is acting on their own behalf.
    if (pid == callingPid && (uid == callingUid || uid == -1)) { // UID can be optional
        return true;
    }
    // Implicitly trust when the caller is our own process.
    if (callingPid == getpid()) {
        return true;
    }
    // Implicitly trust when a media process is calling.
    if (callingUid == AID_MEDIA) {
        return true;
    }
    // Otherwise, allow the caller to act as another process when the caller has permissions.
    return checkCallingPermission(String16("android.permission.MEDIA_RESOURCE_OVERRIDE_PID"));
}

bool ProcessInfo::overrideProcessInfo(int pid, int procState, int oomScore) {
    std::scoped_lock lock{mOverrideLock};

    mOverrideMap.erase(pid);

    // Disable the override if oomScore is set to NATIVE_ADJ or below.
    if (oomScore <= NATIVE_ADJ) {
        return false;
    }

    mOverrideMap.emplace(pid, ProcessInfoOverride{procState, oomScore});
    return true;
}

void ProcessInfo::removeProcessInfoOverride(int pid) {
    std::scoped_lock lock{mOverrideLock};

    mOverrideMap.erase(pid);
}

ProcessInfo::~ProcessInfo() {}

}  // namespace android
