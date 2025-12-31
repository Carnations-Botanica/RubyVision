//
//  AtiDbg2000.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/26/25.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include "AtiDbg2000.hpp"
#include "kern_start.hpp"

mach_vm_address_t AtiDbg2000::orgGetChipID = 0;
mach_vm_address_t AtiDbg2000::orgKlogInitLogLevels = 0;
mach_vm_address_t AtiDbg2000::orgWindowModeToAtiFormat = 0;
mach_vm_address_t AtiDbg2000::logLevelMasksAddr = 0;
size_t AtiDbg2000::logLevelMasksCount = 0;

static const char *pathATIRadeonX2000[] { "/System/Library/Extensions/ATIRadeonX2000.kext/Contents/MacOS/ATIRadeonX2000" };

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.ATIRadeonX2000", pathATIRadeonX2000, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

void AtiDbg2000::init() {
    DBGLOG("AtiDbg2000", "Initializing ATIRadeonX2000 hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);

}

void AtiDbg2000::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (index == kextList[0].loadIndex) {
        DBGLOG("AtiDbg2000", "Found com.apple.ATIRadeonX2000");

        KernelPatcher::RouteRequest requests[] {
            { "__ZN14ATIRadeonX20009getChipIDEv", getChipID, orgGetChipID },
            { "__Z17klogInitLogLevelsv", klogInitLogLevels, orgKlogInitLogLevels },
            { "_window_mode_to_ati_format", windowModeToAtiFormat, orgWindowModeToAtiFormat }
        };

        if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
            DBGLOG("AtiDbg2000", "Failed to hook functions in ATIRadeonX2000");
        } else {
            DBGLOG("AtiDbg2000", "Successfully hooked ATIRadeonX2000 functions");
            
            // Try to resolve _g_pLogLevelMasks by symbol
            logLevelMasksAddr = patcher.solveSymbol(index, "_g_pLogLevelMasks");
            
            // Fallback: Parse instructions if symbol is missing
            if (!logLevelMasksAddr && orgKlogInitLogLevels) {
                DBGLOG("AtiDbg2000", "Symbol _g_pLogLevelMasks not found, scanning instructions...");
                
                // Scan the first 128 bytes of the function for 'mov eax, ADDRESS' (0xB8)
                uint8_t *funcBytes = reinterpret_cast<uint8_t *>(orgKlogInitLogLevels);
                for (size_t i = 0; i < 128; i++) {
                    if (funcBytes[i] == 0xB8) {
                        mach_vm_address_t potentialAddr = *reinterpret_cast<uint32_t *>(funcBytes + i + 1);
                        // Verify address is within the kext range
                        if (potentialAddr > address && potentialAddr < address + size) {
                            logLevelMasksAddr = potentialAddr;
                            DBGLOG("AtiDbg2000", "Found _g_pLogLevelMasks via instruction scan at offset 0x%lX: 0x%08X", i, logLevelMasksAddr);
                            
                            // Try to find the count via 'cmp eax, END_ADDRESS' (0x3D)
                            for (size_t j = i + 5; j < 128; j++) {
                                if (funcBytes[j] == 0x3D) {
                                    mach_vm_address_t endAddr = *reinterpret_cast<uint32_t *>(funcBytes + j + 1);
                                    if (endAddr > logLevelMasksAddr && endAddr < address + size) {
                                        logLevelMasksCount = (endAddr - logLevelMasksAddr) / sizeof(uint32_t);
                                        DBGLOG("AtiDbg2000", "Found array size via CMP instruction: %lu entries", logLevelMasksCount);
                                        break;
                                    }
                                }
                            }
							
							// mmm
							break;
                        }
                    }
                }
            }

            // Default to 19 entries if we found the address but not the size
            if (logLevelMasksAddr && logLevelMasksCount == 0) {
                logLevelMasksCount = 19;
            }
        }
    }
}

uint32_t AtiDbg2000::getChipID(void *that) {
    uint32_t chipID = 0;
    if (orgGetChipID) {
        chipID = FunctionCast(getChipID, orgGetChipID)(that);
    }

    DBGLOG("AtiDbg2000", "ATIRadeonX2000::getChipID() got ChipID: 0x%04X", chipID);
    
    return chipID;
}

void AtiDbg2000::klogInitLogLevels() {
    if (orgKlogInitLogLevels) {
        FunctionCast(klogInitLogLevels, orgKlogInitLogLevels)();
    }
    
    // attempt to overwrite masks to enable all logging
    // after more RE, this is basically useless, there are no
    // logs made, this would be a release kext, RIP
    if (logLevelMasksAddr && logLevelMasksCount > 0) {
        DBGLOG("AtiDbg2000", "Overwriting %lu KLOG masks at 0x%08X to 0xFFFFFFFF", logLevelMasksCount, logLevelMasksAddr);
        
        uint32_t *masks = reinterpret_cast<uint32_t *>(logLevelMasksAddr);
        for (size_t i = 0; i < logLevelMasksCount; i++) {
            masks[i] = 0xFFFFFFFF;
        }
    } else {
        // doesn't frankly matter, it's fine
        DBGLOG("AtiDbg2000", "Cannot enable full KLOG: Address 0x%08X, Count %lu", logLevelMasksAddr, logLevelMasksCount);
    }
	
}

uint32_t AtiDbg2000::windowModeToAtiFormat(uint32_t mode) {
    uint32_t result = 0;
    if (orgWindowModeToAtiFormat) {
        result = FunctionCast(windowModeToAtiFormat, orgWindowModeToAtiFormat)(mode);
    }
    
    // Only log the first 100 calls to prevent log spam/lag
    static uint32_t callCount = 0;
    if (callCount < 100) {
        DBGLOG("AtiDbg2000", "window_mode_to_ati_format called with: 0x%X -> returning result: 0x%X", mode, result);
        callCount++;
        
        if (callCount == 100) {
            DBGLOG("AtiDbg2000", "window_mode_to_ati_format logging limit reached.");
        }
    }
    
    return result;
}
