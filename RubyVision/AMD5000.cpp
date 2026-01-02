//
//  AMD5000.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/31/25.
//

#include "AMD5000.hpp"

// Internal structure helper based on ASM
struct BiosContainer {
    void* buffer;      // offset 0
    uint32_t size;     // offset 8
    // padding likely exists between 0 and 8 on 64-bit
};

// Initialize trampolines
mach_vm_address_t AMD5000::orgGetDeviceId = 0;
mach_vm_address_t AMD5000::orgReadATOMBIOS = 0;
mach_vm_address_t AMD5000::orgReadEfiRom = 0;
mach_vm_address_t AMD5000::orgReadVramRom = 0;
mach_vm_address_t AMD5000::orgReadPciRom = 0;
mach_vm_address_t AMD5000::orgReadRegRom = 0;
mach_vm_address_t AMD5000::orgGetBIOSInfo = 0;
mach_vm_address_t AMD5000::orgMapPCISubRange = 0;

// Define paths
static const char *pathAMD5000Controller[] { "/System/Library/Extensions/ATI5000Controller.kext/Contents/MacOS/AMD5000Controller" };
static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.AMD5000Controller", pathAMD5000Controller, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

// Main AMD5000 Routine
void AMD5000::init() {
    DBGLOG(AMD5K, "Initializing AMD5000Controller hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

// Main hook routine
void AMD5000::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    size_t kextIndex = arrsize(kextList);
    for (size_t i = 0; i < arrsize(kextList); i++) {
        if (kextList[i].loadIndex == index) {
            kextIndex = i;
            break;
        }
    }
    if (kextIndex == arrsize(kextList)) {
        return;
    }
    if (strcmp(kextList[kextIndex].id, "com.apple.kext.AMD5000Controller") != 0) {
        return;
    }

    DBGLOG(AMD5K, "Found %s (of Global Index %lu) - Hooking...", kextList[kextIndex].id, index);

    KernelPatcher::RouteRequest requests[] {
        { "__ZN17AMD5000Controller11getDeviceIdEv", getDeviceId, orgGetDeviceId },
        { "__ZN17AMD5000Controller12readATOMBIOSEv", readATOMBIOS, orgReadATOMBIOS },
        { "__ZN17AMD5000Controller11getBIOSInfoEv", getBIOSInfo, orgGetBIOSInfo },
        { "__ZN17AMD5000Controller10readEfiRomEPhj", readEfiRom, orgReadEfiRom },
        { "__ZN17AMD5000Controller11readVramRomEPhj", readVramRom, orgReadVramRom },
        { "__ZN17AMD5000Controller10readPciRomEPhj", readPciRom, orgReadPciRom },
        { "__ZN17AMD5000Controller10readRegRomEPhj", readRegRom, orgReadRegRom },
        { "__ZN13ATIController14mapPCISubRangeE13PCI_REG_INDEXyy", nullptr, orgMapPCISubRange }
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG(AMD5K, "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG(AMD5K, "Successfully hooked functions for Index %lu.", index);
    }

}

void getProcInfo(char *name, int &pid) {
    proc_selfname(name, 64);
    pid = proc_selfpid();
}

// Helper for various ROM Reads
void logRomAttempt(const char *method, uint8_t *buffer, uint32_t length, int result) {
    char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    
    bool signatureFound = false;
    if (buffer && length >= 2) {
        if (buffer[0] == 0x55 && buffer[1] == 0xAA) {
            signatureFound = true;
        }
    }

    DBGLOG(AMD5K, "[%s:%d] %s(len=0x%X) -> Returned: 0x%X. Signature Valid: %s", 
           pName, pid, method, length, result, signatureFound ? "YES" : "NO");

    if (buffer && length >= 4 && signatureFound) {
        DBGLOG(AMD5K, "    Buffer Dump: %02X %02X %02X %02X", buffer[0], buffer[1], buffer[2], buffer[3]);
    }

}

int AMD5000::readEfiRom(void *that, uint8_t *buffer, uint32_t length) {
    // char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    // IOService *controller = static_cast<IOService *>(that);
    
    // if (controller) {
    //     IOService *provider = controller->getProvider();
    //     if (provider) {
    //         OSObject *prop = provider->getProperty("ATY,bin_image");
    //         if (!prop) {
    //             DBGLOG("AtiDbg5000", "[%s:%d] readEfiRom Pre-Check: 'ATY,bin_image' property NOT found.", pName, pid);
    //         } else {
    //             OSData *data = OSDynamicCast(OSData, prop);
    //             if (!data) {
    //                 DBGLOG("AtiDbg5000", "[%s:%d] readEfiRom Pre-Check: 'ATY,bin_image' is NOT OSData.", pName, pid);
    //             } else {
    //                 uint32_t propLen = data->getLength();
    //                 DBGLOG("AtiDbg5000", "[%s:%d] readEfiRom Pre-Check: 'ATY,bin_image' found. Size: %d", pName, pid, propLen);
                    
    //                 if (propLen >= 2) {
    //                     const uint8_t *bytes = (const uint8_t *)data->getBytesNoCopy();
    //                     if (bytes[0] != 0x55 || bytes[1] != 0xAA) {
    //                         DBGLOG("AtiDbg5000", "    WARN: Property contains Invalid ROM Signature (%02X %02X)!", bytes[0], bytes[1]);
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    if (orgReadEfiRom) {
        typedef int (*t_read)(void *, uint8_t *, uint32_t);
        int ret = reinterpret_cast<t_read>(orgReadEfiRom)(that, buffer, length);
        logRomAttempt("readEfiRom", buffer, length, ret);
        return ret;
    }

    return 0;
}

int AMD5000::readVramRom(void *that, uint8_t *buffer, uint32_t length) {
    // char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    // IOService *controller = static_cast<IOService *>(that);
    // IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, controller ? controller->getProvider() : nullptr);
    
    // if (pci) {
    //     // Read BAR 0 (Memory) and Command Register
    //     uint32_t bar0 = pci->configRead32(0x10);
    //     uint16_t cmd = pci->configRead16(0x04);
    //     bool memEnabled = (cmd & 0x02) != 0;
    
    //     DBGLOG("AtiDbg5000", "[%s:%d] readVramRom Pre-Check: BAR0=0x%08X, CmdReg=0x%04X (MemEnable=%s)", 
    //            pName, pid, bar0, cmd, memEnabled ? "YES" : "NO");
    
    //     if (!memEnabled) {
    //         DBGLOG("AtiDbg5000", "[%s:%d] readVramRom Warning: Memory decoding disabled! Attempting to enable...", pName, pid);
    //         pci->configWrite16(0x04, cmd | 0x02);
    //     }
    // }

    if (orgReadVramRom) {
        typedef int (*t_read)(void *, uint8_t *, uint32_t);
        int ret = reinterpret_cast<t_read>(orgReadVramRom)(that, buffer, length);
        logRomAttempt("readVramRom", buffer, length, ret);
        return ret;
    }

    return 0;
}

int AMD5000::readPciRom(void *that, uint8_t *buffer, uint32_t length) {
    // char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    // IOService *controller = static_cast<IOService *>(that);
    // IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, controller ? controller->getProvider() : nullptr);
    
    // if (pci) {
    //     // Read Expansion ROM BAR
    //     uint32_t romBar = pci->configRead32(0x30);
    //     DBGLOG("AtiDbg5000", "[%s:%d] readPciRom Pre-Check: Expansion ROM BAR=0x%08X", pName, pid, romBar);
        
    //     if ((romBar & 0xFFFFF800) == 0) {
    //          DBGLOG("AtiDbg5000", "[%s:%d] readPciRom Warning: ROM BAR appears empty/unassigned.", pName, pid);
    //     }
    // }

    if (orgReadPciRom) {
        typedef int (*t_read)(void *, uint8_t *, uint32_t);
        int ret = reinterpret_cast<t_read>(orgReadPciRom)(that, buffer, length);
        logRomAttempt("readPciRom", buffer, length, ret);
        return ret;
    }

    return 0;
}

int AMD5000::readRegRom(void *that, uint8_t *buffer, uint32_t length) {
    if (orgReadRegRom) {
        typedef int (*t_read)(void *, uint8_t *, uint32_t);
        int ret = reinterpret_cast<t_read>(orgReadRegRom)(that, buffer, length);
        logRomAttempt("readRegRom", buffer, length, ret);
        
        // Patch VBIOS checksum & verify ATOM bios
        if (buffer && length > 0) {
            // Scan for "ATOM" signature
            bool atomFound = false;
            for (uint32_t i = 0; i < length - 4; i++) {
                if (buffer[i] == 'A' && buffer[i+1] == 'T' && buffer[i+2] == 'O' && buffer[i+3] == 'M') {
                    DBGLOG("AtiDbg5000", "    ATOM Signature found at offset 0x%X", i);
                    atomFound = true;
                    break;
                }
            }
            
            if (!atomFound) {
                 DBGLOG(AMD5K, "    WARN: ATOM Signature NOT found in VBIOS image!");
            }

            // Validate and Fix Checksum
            uint8_t sum = 0;
            for (uint32_t i = 0; i < length; i++) {
                sum += buffer[i];
            }
            
            if (sum != 0) {
                DBGLOG(AMD5K, "    WARN: Invalid VBIOS Checksum (Sum: 0x%02X). Fixing...", sum);
                // Calculate difference needed to make sum == 0
                uint8_t diff = 0 - sum;
                buffer[length - 1] += diff;
                DBGLOG(AMD5K, "    VBIOS Checksum Fixed.");
            } else {
                DBGLOG(AMD5K, "    VBIOS Checksum Valid (0x00).");
            }
        }
        
        return ret;
    }

    return 0;
}

int AMD5000::readATOMBIOS(void *that) {
    DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): called");

    // Offset 0x418: BiosContainer*
    BiosContainer* biosInfo = getMember<BiosContainer*>(that, 0x418);
    
    if (!biosInfo) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Error - BiosInfo struct is null");
        return kIOReturnError;
    }

    // ASM 0x163af: If buffer is already allocated, return kIOReturnSuccess (0) immediately.
    // This prevents re-reading or double-allocation logic issues.
    if (biosInfo->buffer != nullptr) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): BIOS already loaded. Returning success.");
        return kIOReturnSuccess;
    }

    // Allocate 64KB buffer
    biosInfo->buffer = IOMalloc(0x10000);
    biosInfo->size = 0x10000;

    void* buffer = biosInfo->buffer;
    uint32_t size = biosInfo->size;

    if (!buffer) return kIOReturnNoMemory;

    bool success = false;

    // Try standard readers
    // In ASM, these return non-zero on success.
    if (readEfiRom(that, (uint8_t*)buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from EFI ROM");
        success = true;
    } else if (readVramRom(that, (uint8_t*)buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from VRAM ROM");
        success = true;
    } else if (readPciRom(that, (uint8_t*)buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from PCI ROM");
        success = true;
    } else if (readRegRom(that, (uint8_t*)buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from Registry ROM");
        success = true;
    }

    // Fallback & Property Sync
    IOService* provider = getMember<IOService*>(that, 0x160);
    
    if (provider) {
        OSObject* prop = provider->getProperty("ATY,bin_image");
        OSData* data = OSDynamicCast(OSData, prop);
        
        if (data && data->getLength() > 0) {
            // If we haven't loaded successfully yet, use this data
            if (!success) {
                size_t copySize = (data->getLength() > size) ? size : data->getLength();
                memcpy(buffer, data->getBytesNoCopy(), copySize);
                DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(Fallback): Loaded BIOS from ATY,bin_image");
                success = true;
            }
        } else if (success) {
            // If we HAVE loaded successfully, but property is missing, create it.
            OSData* newData = OSData::withBytes(buffer, size);
            if (newData) {
                provider->setProperty("ATY,bin_image", newData);
                newData->release();
                DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Created ATY,bin_image property on provider.");
            }
        }
    } else {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Provider at 0x160 is null.");
    }

    // Failure handling
    if (!success) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Failed to load BIOS from any source!");
        if (biosInfo->buffer) {
            IOFree(biosInfo->buffer, biosInfo->size);
            biosInfo->buffer = nullptr;
            biosInfo->size = 0;
        }
        return kIOReturnNotFound; // 0xe00002c9
    }
    
    return getBIOSInfo(that);
}

int AMD5000::getBIOSInfo(void *that) {
    // Offset 0x418
    BiosContainer* biosInfo = getMember<BiosContainer*>(that, 0x418);
    if (!biosInfo || !biosInfo->buffer) return kIOReturnNotFound;

    uint8_t* rom = (uint8_t*)biosInfo->buffer;
    
    // Offset 0x509 (r15 = r14 + 0x509)
    char* versionBuf = (char*)((uintptr_t)that + 0x509);
    strncpy(versionBuf, "XXX-XXXXX-XXX", 0x20);

    // Header offset
    uint16_t headerOffset = rom[0x48] | (rom[0x49] << 8);
    
    // Safety check (ASM 0x16006)
    if (headerOffset + 4 > 0xFFFF) return 0xe00002c7;

    uint32_t signature = *reinterpret_cast<uint32_t*>(rom + headerOffset + 4);
    
    if (signature == 0x4D4F5441) { // 'ATOM'
        uint8_t strOffset = rom[headerOffset + 0x6e];

        bool foundSku = false;
        for (int i = 0; i < 512; i++) {
            if (rom[i] == '1' && rom[i+1] == '1' && rom[i+2] == '3' && rom[i+3] == '-') {
                strncpy(versionBuf, (char*)(rom + i), 0x20);
                foundSku = true;
                break;
            }
        }

        if (!foundSku && strOffset != 0) {
            strncpy(versionBuf, (char*)(rom + strOffset), 0x20);
        }

        uint8_t& flag = getMember<uint8_t>(that, 0x528);
        
        if (flag != 0) {
            flag = 0;
            DBGLOG(AMD5K, "AMD5000Controller::getBIOSInfo(): Flag 0x528 was set. Clearing and returning Error.");
            return 0xe00002c7;
        }
        
        return kIOReturnSuccess;
    }

    return 0xe00002c7;
}

uint32_t AMD5000::getDeviceId(void *that) {
    uint32_t deviceId = 0;
    char procName[64] = {0};
    proc_selfname(procName, sizeof(procName));
    int pid = proc_selfpid();
    DBGLOG(AMD5K, "[%s:%d] AMD5000Controller::getDeviceId() called.", procName, pid);
    
    // Get the real Device ID from the hardware/original function
    if (orgGetDeviceId) {
        deviceId = FunctionCast(getDeviceId, orgGetDeviceId)(that);
        DBGLOG(AMD5K, "[%s:%d] AMD5000Controller::getDeviceId() got Device ID: 0x%04X", procName, pid, deviceId);
    } else {
        deviceId = 0x0000;
        DBGLOG(AMD5K, "AMD5000Controller::getDeviceId() failed to call via trampoline!");
        return deviceId;
    }

    // Spoof injected devices to architecture match
    uint32_t originalId = deviceId;
    if ((deviceId & 0xFFFF) == 0x68F9) {
        deviceId = (deviceId & 0xFFFF0000) | 0x68E0;
    }

    // We log out if a card supported by RV is being actively spoofed
    if ((originalId & 0xFFFF) == 0x68F9) {
        DBGLOG(AMD5K, "    Real ID: 0x%04X -> Spoofed ID: 0x%04X", originalId, deviceId);
    }

    return deviceId;
}

