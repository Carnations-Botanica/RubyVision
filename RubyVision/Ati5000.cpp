//
//  Ati5000.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/28/25.
//

#include "Ati5000.hpp"

// Initialize static members
mach_vm_address_t Ati5000::orgGetDeviceId = 0;

// Define paths for targeted kexts
static const char *pathATI5000Controller[] { "/System/Library/Extensions/ATI5000Controller.kext/Contents/MacOS/ATI5000Controller" };
static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.ATI5000Controller", pathATI5000Controller, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

// Main Ati5000 Routine
void Ati5000::init() {
    DBGLOG(ATI5K, "Initializing hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

// Main 5000Controller hook routine
void Ati5000::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    DBGLOG("Ati5000", "Found ATI5000Controller (of Index %lu)", index);

    KernelPatcher::RouteRequest requests[] {
        { "__ZN17AMD5000Controller11getDeviceIdEv", getDeviceId, orgGetDeviceId },
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG(ATI5K, "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG(ATI5K, "Successfully hooked functions for Index %lu.", index);
    }

    // todo
}

uint32_t Ati5000::getDeviceId(void *that) {
    uint32_t deviceId = 0;
    if (orgGetDeviceId) {
        deviceId = FunctionCast(getDeviceId, orgGetDeviceId)(that);
    }

    char procName[64] = {0};
    proc_selfname(procName, sizeof(procName));
    int pid = proc_selfpid();
    DBGLOG("Ati5000", "[%s:%d] ATI5000Controller::getDeviceId() called by %p which got DeviceID: 0x%04X", procName, pid, that, deviceId);

    return deviceId;
}
