//
//  AtiDbgSupport.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/29/25.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include "AtiDbgSupport.hpp"
#include "kern_start.hpp"

// Initialize static members
mach_vm_address_t AtiDbgSupport::orgGetConnectorsInfo = 0;
mach_vm_address_t AtiDbgSupport::orgCreateAtomBiosProxy = 0;

// Define paths for ATI/AMDSupport
// static const char *pathAMDSupport[] { "/System/Library/Extensions/AMDSupport.kext/Contents/MacOS/AMDSupport" };
static const char *pathATISupport[] { "/System/Library/Extensions/ATISupport.kext/Contents/MacOS/AMDSupport" };

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.ATISupport", pathATISupport, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
    // { "com.apple.kext.AMDSupport", pathAMDSupport, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

void AtiDbgSupport::init() {
    DBGLOG("AtiDbgSupp", "Initializing AMDSupport hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

void AtiDbgSupport::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    // Check which ID matched to print correct log, though not strictly necessary, helps me not go insane during these periods
    if (index == kextList[0].loadIndex) {
        DBGLOG("AtiDbgSupp", "Found AMDSupport (Modern)");
    } else {
        DBGLOG("AtiDbgSupp", "Found ATISupport (Legacy/Other)");
    }

    KernelPatcher::RouteRequest requests[] {
        // AtomBiosProxy::getConnectorsInfo(ConnectorInfo*, unsigned char&)
        // { "__ZN13AtomBiosProxy17getConnectorsInfoEP13ConnectorInfoRh", getConnectorsInfo, orgGetConnectorsInfo },
        // AtomBiosProxy::createAtomBiosProxy(unsigned int, unsigned char*, ARegisterService*, MemoryAllocator*)
        { "__ZN13AtomBiosProxy19createAtomBiosProxyEjPhP16ARegisterServiceP15MemoryAllocator", createAtomBiosProxy, orgCreateAtomBiosProxy }
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG("AtiDbgSupp", "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG("AtiDbgSupp", "Successfully hooked functions for Index %lu.", index);
    }
}

void *AtiDbgSupport::createAtomBiosProxy(uint32_t length, uint8_t *buffer, void *service, void *allocator) {
    DBGLOG("AtiDbgSupp", "createAtomBiosProxy called with Length: 0x%X", length);
    
    // Dump the first few bytes to verify what we are passing to the factory
    if (buffer && length >= 4) {
        DBGLOG("AtiDbgSupp", "  VBIOS Header: %02X %02X %02X %02X", buffer[0], buffer[1], buffer[2], buffer[3]);
        
        // Verify ATOM signature inside the passed buffer
        bool atomFound = false;
        for (uint32_t i = 0; i < (length - 4); i++) {
            if (buffer[i] == 'A' && buffer[i+1] == 'T' && buffer[i+2] == 'O' && buffer[i+3] == 'M') {
                DBGLOG("AtiDbgSupp", "  ATOM Signature verified at offset 0x%X", i);
                atomFound = true;
                break;
            }
        }
        if (!atomFound) {
            DBGLOG("AtiDbgSupp", "  CRITICAL: ATOM Signature NOT found in buffer passed to Proxy!");
        }
    } else {
        DBGLOG("AtiDbgSupp", "  CRITICAL: Buffer is NULL or Length is too small!");
    }

    // Call Original
    void *proxy = nullptr;
    if (orgCreateAtomBiosProxy) {
        proxy = FunctionCast(createAtomBiosProxy, orgCreateAtomBiosProxy)(length, buffer, service, allocator);
    }

    if (proxy) {
        DBGLOG("AtiDbgSupp", "createAtomBiosProxy: SUCCESS! Proxy created at %p", proxy);
    } else {
        DBGLOG("AtiDbgSupp", "createAtomBiosProxy: FAILED! Returned NULL.");
    }
    
    return proxy;
}

int AtiDbgSupport::getConnectorsInfo(void *that, ConnectorInfo *infoBuffer, uint8_t *countRef) {
    int result = 0;
    
    // Call the original function to let AMDSupport parse the VBIOS
    if (orgGetConnectorsInfo) {
        result = FunctionCast(getConnectorsInfo, orgGetConnectorsInfo)(that, infoBuffer, countRef);
    } else {
        DBGLOG("AtiDbgSupp", "getConnectorsInfo: Original trampoline is null!");
        return 0xE00002C2;
    }

    // Log the results
    if (result == 0 && infoBuffer && countRef) {
        uint8_t count = *countRef;
        DBGLOG("AtiDbgSupp", "getConnectorsInfo called. Count: %d", count);

        for (uint8_t i = 0; i < count; i++) {
            ConnectorInfo &cn = infoBuffer[i];
            
            DBGLOG("AtiDbgSupp", "  [%d] Type: 0x%08X | Flags: 0x%08X", i, cn.type, cn.flags);
            DBGLOG("AtiDbgSupp", "       Trans: 0x%02X | Enc: 0x%02X | Sense: 0x%02X | HP: 0x%02X",
                   cn.transmitter, cn.encoder, cn.senseId, cn.hotplugId);
            
            const char *typeStr = "Unknown";
            if (cn.type == 0x00040000) typeStr = "DisplayPort";
            else if (cn.type == 0x00080000) typeStr = "HDMI";
            else if (cn.type == 0x00020000) typeStr = "DVI-D (Single)";
            else if (cn.type == 0x00040000) typeStr = "DVI-D (Dual)";
            else if (cn.type == 0x00000010) typeStr = "VGA";
            else if (cn.type == 0x00000001) typeStr = "LVDS";
            
            DBGLOG("AtiDbgSupp", "       -> Likely: %s", typeStr);
        }
    } else {
        DBGLOG("AtiDbgSupp", "getConnectorsInfo failed or returned no data. Result: 0x%X", result);
    }

    return result;
}
