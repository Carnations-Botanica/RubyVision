//
//  AtiDbg5000.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/28/25.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <sys/proc.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <mach/vm_map.h>
#include "AtiDbg5000.hpp"
#include "kern_start.hpp"

// Initialize static members
mach_vm_address_t AtiDbg5000::orgGetDeviceId = 0;
mach_vm_address_t AtiDbg5000::orgReadATOMBIOS = 0;
mach_vm_address_t AtiDbg5000::orgReadEfiRom = 0;
mach_vm_address_t AtiDbg5000::orgReadVramRom = 0;
mach_vm_address_t AtiDbg5000::orgReadPciRom = 0;
mach_vm_address_t AtiDbg5000::orgReadRegRom = 0;
mach_vm_address_t AtiDbg5000::orgGetBIOSInfo = 0;
mach_vm_address_t AtiDbg5000::orgInitializeProjectInfo = 0;

// Define paths for targeted kexts
static const char *pathATI5000Controller[] { "/System/Library/Extensions/ATI5000Controller.kext/Contents/MacOS/AMD5000Controller" };
static const char *pathAMD5000Controller[] { "/System/Library/Extensions/AMD5000Controller.kext/Contents/MacOS/AMD5000Controller" };

// Watch for both potential IDs to make sure we catch it
static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.AMD5000Controller", pathATI5000Controller, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
    // { "com.apple.kext.AMD5000Controller", pathAMD5000Controller, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

// Helper for getting Proc name, clearly
void getProcInfo(char *name, int &pid) {
    proc_selfname(name, 64);
    pid = proc_selfpid();
}

// Represents the VBIOS container at offset 0x418
struct VBIOSContainer {
    uint8_t *buffer; // 0x0
    uint32_t length; // 0x8
};

void AtiDbg5000::init() {
    DBGLOG("AtiDbg5000", "Initializing hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

// Original Hoolock (3) @ 0x29300
// DP, DP, DDVI
// 00 04 00 00 04 06 00 00 00 01 00 00 21 03 05 01
// 00 04 00 00 04 06 00 00 00 01 00 00 11 02 04 02
// 04 00 00 00 14 02 00 00 00 01 00 00 02 04 01 03
static const uint8_t originalHoolockBytes[] = {
    // Port 1 (DP)
    0x00, 0x08, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x21, 0x03, 0x05, 0x01,
    // Port 2 (DP)
    0x00, 0x04, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x11, 0x02, 0x04, 0x02,
    // Port 3 (DDVI)
    0x04, 0x00, 0x00, 0x00, 0x14, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x04, 0x01, 0x03
};

// Custom Hoolock Connector Data
// HDMI, DVI, VGA
// 00 08 00 00 04 02 00 00 00 01 00 00 21 03 05 01 
// 00 04 00 00 04 06 00 00 00 01 00 00 11 02 04 02
// 04 00 00 00 14 02 00 00 00 01 00 00 02 04 01 03
static const uint8_t customHoolockBytes[] = {
    // Port 1 (HDMI)
    0x00, 0x08, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x21, 0x03, 0x05, 0x01,
    // Port 2 (DP)
    0x00, 0x04, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x11, 0x02, 0x04, 0x02,
    // Port 3 (DDVI)
    0x04, 0x00, 0x00, 0x00, 0x14, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x04, 0x01, 0x03
};

// Helper to patch data in memory
void patchHoolockConnectors(mach_vm_address_t kextBase, size_t kextSize) {
    uint8_t *base = reinterpret_cast<uint8_t *>(kextBase);
    const size_t patternSize = sizeof(originalHoolockBytes);

    DBGLOG("AtiDbg5000", "Scanning for Hoolock Connector Table in range: 0x%llX - 0x%llX", kextBase, kextBase + kextSize);

    // Scan memory for the pattern
    for (size_t i = 0; i < kextSize - patternSize; i++) {
        if (memcmp(base + i, originalHoolockBytes, patternSize) == 0) {
            mach_vm_address_t foundAddr = kextBase + i;
            DBGLOG("AtiDbg5000", "FOUND Hoolock Connectors at Offset 0x%lX (Addr: 0x%llX)", i, foundAddr);

            vm_protect(kernel_map, foundAddr, patternSize, FALSE, VM_PROT_READ | VM_PROT_WRITE);
            memcpy(base + i, customHoolockBytes, patternSize);
            DBGLOG("AtiDbg5000", "PATCHED Hoolock Connectors with custom HDMI/DVI/VGA table.");
            
            return;
        }
    }
    
    DBGLOG("AtiDbg5000", "ERROR: Hoolock Connector Table NOT found!");
}

// Main 5000Controller hook routine
void AtiDbg5000::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    DBGLOG("AtiDbg5000", "Found ATI/AMD5000Controller (of Index %lu)", index);

    KernelPatcher::RouteRequest requests[] {
        { "__ZN17AMD5000Controller11getDeviceIdEv", getDeviceId, orgGetDeviceId },
        { "__ZN17AMD5000Controller12readATOMBIOSEv", readATOMBIOS, orgReadATOMBIOS },
        { "__ZN17AMD5000Controller10readEfiRomEPhj", readEfiRom, orgReadEfiRom },
        { "__ZN17AMD5000Controller11readVramRomEPhj", readVramRom, orgReadVramRom },
        { "__ZN17AMD5000Controller10readPciRomEPhj", readPciRom, orgReadPciRom },
        { "__ZN17AMD5000Controller10readRegRomEPhj", readRegRom, orgReadRegRom },
        { "__ZN17AMD5000Controller11getBIOSInfoEv", getBIOSInfo, orgGetBIOSInfo },
        { "__ZN17AMD5000Controller21initializeProjectInfoEv", initializeProjectInfo, orgInitializeProjectInfo }
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG("AtiDbg5000", "Failed to hook Controller functions.");
    } else {
        DBGLOG("AtiDbg5000", "Successfully hooked Controller functions.");
    }

    // Perform Binary Patches for connectors
    patchHoolockConnectors(address, size); // only one atm

}

void logRomAttempt(const char *method, uint8_t *buffer, uint32_t length, int result) {
    char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    
    bool signatureFound = false;
    if (buffer && length >= 2) {
        if (buffer[0] == 0x55 && buffer[1] == 0xAA) {
            signatureFound = true;
        }
    }

    DBGLOG("AtiDbg5000", "[%s:%d] %s(len=0x%X) -> Returned: 0x%X. Signature Valid: %s", 
           pName, pid, method, length, result, signatureFound ? "YES" : "NO");

    if (buffer && length >= 4 && signatureFound) {
        DBGLOG("AtiDbg5000", "    Buffer Dump: %02X %02X %02X %02X", buffer[0], buffer[1], buffer[2], buffer[3]);
    }

}

uint32_t AtiDbg5000::getDeviceId(void *that) {
    uint32_t deviceId = 0;
    char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);

    if (orgGetDeviceId) {
        deviceId = FunctionCast(getDeviceId, orgGetDeviceId)(that);
    }
    
    DBGLOG("AtiDbg5000", "[%s:%d] getDeviceId got 0x%04X", pName, pid, deviceId);
    if (deviceId == 0x68F9) {
        deviceId = 0x68E0; // Spoof to Apple Radeon HD 5450
        DBGLOG("AtiDbg5000", "[%s:%d] Spoofed DeviceID 0x68F9 -> 0x68E0", pName, pid);
    }

    return deviceId;
}

int AtiDbg5000::readATOMBIOS(void *that) {
    char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    DBGLOG("AtiDbg5000", "[%s:%d] AtiDbg5000::readATOMBIOS() called!", pName, pid);

    int result = 0;
    if (orgReadATOMBIOS) {
        result = FunctionCast(readATOMBIOS, orgReadATOMBIOS)(that);
    } else {
        DBGLOG("AtiDbg5000", "readATOMBIOS: Original function not found!");
        return 0xE00002C2;
    }
    
    IOService *controller = static_cast<IOService *>(that);
    IOService *provider = controller ? controller->getProvider() : nullptr;
    
    if (controller && provider) {
        // Inject RefCLK
        if (!provider->getProperty("ATY,RefCLK")) {
            DBGLOG("AtiDbg5000", "readATOMBIOS: Injecting 'ATY,RefCLK'/'ATY,RefClock' (to 2700)...");
            uint32_t refClk = 2700; // 0x0A8C
            OSData *refClkData = OSData::withBytes(&refClk, sizeof(refClk));
            controller->setProperty("ATY,RefCLK", refClkData);
            controller->setProperty("ATY,RefClock", refClkData);
            refClkData->release();
        }

        // Force Framebuffer Personality
        if (!provider->getProperty("compatible")) {
            DBGLOG("AtiDbg5000", "readATOMBIOS: Forcing Framebuffer 'ATY,Hoolock'...");
            OSString *fb = OSString::withCString("ATY,Hoolock");
            controller->setProperty("compatible", fb);

            fb->release();
        }

    } else {
        DBGLOG("AtiDbg5000", "readATOMBIOS: ERROR - Controller or Provider is NULL!");
    }

    DBGLOG("AtiDbg5000", "readATOMBIOS: Original returned 0x%X", result);
    return result;
}

int AtiDbg5000::getBIOSInfo(void *that) {
    uint8_t *thatPtr = static_cast<uint8_t *>(that);
    VBIOSContainer **containerPtr = reinterpret_cast<VBIOSContainer **>(thatPtr + 0x418);
    VBIOSContainer *container = *containerPtr;
    
    if (container && container->buffer) {
        uint8_t *rom = container->buffer;
        uint16_t atomOffset = (rom[0x49] << 8) | rom[0x48];
        uint32_t signature = *reinterpret_cast<uint32_t *>(rom + atomOffset + 4);
        
        DBGLOG("AtiDbg5000", "getBIOSInfo Check: ATOM Offset=0x%04X, Signature at +4=0x%08X (Expected 0x4D4F5441 'ATOM')", 
               atomOffset, signature);
        
        if (signature == 0x4D4F5441) { // 'ATOM'
            uint8_t strOffset = rom[0x6E];
            DBGLOG("AtiDbg5000", "getBIOSInfo Check: String Offset Byte at 0x6E = 0x%02X", strOffset);
            
            if (strOffset != 0) {
                 DBGLOG("AtiDbg5000", "getBIOSInfo Check: String at 0x%02X: %s", strOffset, (char*)(rom + strOffset));
            }
        } else {
             DBGLOG("AtiDbg5000", "getBIOSInfo Check: Signature Mismatch!");
        }
    } else {
        DBGLOG("AtiDbg5000", "getBIOSInfo Check: Container or Buffer is NULL");
    }

    if (orgGetBIOSInfo) {
        typedef int (*t_getInfo)(void *);
        return reinterpret_cast<t_getInfo>(orgGetBIOSInfo)(that);
    }
    
    return 0;
}

int AtiDbg5000::readEfiRom(void *that, uint8_t *buffer, uint32_t length) {
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

int AtiDbg5000::readVramRom(void *that, uint8_t *buffer, uint32_t length) {
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

int AtiDbg5000::readPciRom(void *that, uint8_t *buffer, uint32_t length) {
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

int AtiDbg5000::readRegRom(void *that, uint8_t *buffer, uint32_t length) {
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
                 DBGLOG("AtiDbg5000", "    WARN: ATOM Signature NOT found in VBIOS image!");
            }

            // Validate and Fix Checksum
            uint8_t sum = 0;
            for (uint32_t i = 0; i < length; i++) {
                sum += buffer[i];
            }
            
            if (sum != 0) {
                DBGLOG("AtiDbg5000", "    WARN: Invalid VBIOS Checksum (Sum: 0x%02X). Fixing...", sum);
                // Calculate difference needed to make sum == 0
                uint8_t diff = 0 - sum;
                // Apply to last byte (standard checksum location)
                buffer[length - 1] += diff;
                DBGLOG("AtiDbg5000", "    VBIOS Checksum Fixed.");
            } else {
                DBGLOG("AtiDbg5000", "    VBIOS Checksum Valid (0x00).");
            }
        }
        
        return ret;
    }

    return 0;
}

void AtiDbg5000::initializeProjectInfo(void *that) {
    char pName[64] = {0}; int pid = 0; getProcInfo(pName, pid);
    DBGLOG("AtiDbg5000", "[%s:%d] initializeProjectInfo() called", pName, pid);
    
    // Run original (it will fail to set connectors because Proxy is NULL, but might set other things)
    if (orgInitializeProjectInfo) {
        typedef void (*t_initProj)(void *);
        reinterpret_cast<t_initProj>(orgInitializeProjectInfo)(that);
    }
    
    // Manual Property Override
    // This entire section is just experimenting with devprops
    IOService *ctrl = static_cast<IOService *>(that);
    if (ctrl) {
        DBGLOG("AtiDbg5000", "initializeProjectInfo: Manually injecting 3 connectors...");
        
        char key[32];
        OSString *fbName = OSString::withCString("ATY,Hoolock");
        OSString *devType = OSString::withCString("display");
        
        // We inject 3 connectors to match
        for (int i = 0; i < 3; i++) {
            // @x,compatible = ATY,Hoolock
            snprintf(key, sizeof(key), "@%d,compatible", i);
            ctrl->setProperty(key, fbName);
            
            // @x,device_type = display
            snprintf(key, sizeof(key), "@%d,device_type", i);
            ctrl->setProperty(key, devType);
            
            DBGLOG("AtiDbg5000", "  Set %s and device_type", key);
        }
        
        // Globals are set
        ctrl->setProperty("ATY,Config", fbName);
        ctrl->setProperty("ATY,Personality", fbName);
        if (!ctrl->getProperty("model")) {
             OSString *model = OSString::withCString("AMD Radeon HD 5450");
             ctrl->setProperty("model", model);
             model->release();
        }
        
        fbName->release();
        devType->release();
    }

}
