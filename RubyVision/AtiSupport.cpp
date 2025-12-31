//
//  AtiSupport.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/29/25.
//

#include "AtiSupport.hpp"
#include <sys/proc.h>

// Initialize static members
mach_vm_address_t AtiSupport::orgGetAtomObjectTableForType = 0;

// Define paths
static const char *pathATISupport[] { "/System/Library/Extensions/ATISupport.kext/Contents/MacOS/ATISupport" };

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.ATISupport", pathATISupport, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

void AtiSupport::init() {
    DBGLOG(ATISUP, "Initializing ATISupport hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

void AtiSupport::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    DBGLOG(ATISUP, "Found ATISupport (of Index %lu)", index);
    
    KernelPatcher::RouteRequest requests[] {
        { "__ZN20AtiAtomBiosUtilities25getAtomObjectTableForTypeEhRh", getAtomObjectTableForType, orgGetAtomObjectTableForType },
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG(ATISUP, "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG(ATISUP, "Successfully hooked functions for Index %lu.", index);
    }
}

void *AtiSupport::getAtomObjectTableForType(void *that, uint8_t type, uint8_t &revision) {
    void *result = nullptr;
    
    // Call Original
    if (orgGetAtomObjectTableForType) {
        result = FunctionCast(getAtomObjectTableForType, orgGetAtomObjectTableForType)(that, type, revision);
    }
    
    // Log the interaction
    char procName[64] = {0};
    proc_selfname(procName, sizeof(procName));
    int pid = proc_selfpid();
    
    // Who called it, what type they asked for, what revision was written back, and the resulting pointer
    DBGLOG(ATISUP, "[%s:%d] getAtomObjectTableForType(Type: 0x%02X) -> Ptr: %p, Rev: 0x%02X", procName, pid, type, result, revision);
    
    return result;
}
