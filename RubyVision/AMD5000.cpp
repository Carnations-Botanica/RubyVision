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

bool AMD5000::readEfiRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(AMD5K, "AMD5000Controller::readEfiRom(): readEfiRom called (Size: %d)", size);
    
    // Offset 0x32 is likely the IOPCIDevice* provider
    IOPCIDevice* device = getMember<IOPCIDevice*>(that, 0x32);
    if (!device) return false;

    // Check for "ATY,bin_image" on the provider
    OSObject* prop = device->getProperty("ATY,bin_image");
    OSData* data = OSDynamicCast(OSData, prop);
    
    if (data) {
        uint32_t len = data->getLength();
        DBGLOG(AMD5K, "AMD5000Controller::readEfiRom(): Found ATY,bin_image in EFI (Len: %d)", len);
        
        if (len >= size) {
            len = size;
        } else {
            // If data is smaller, use what we have? Original code seems to imply strict size check
            // but we can be lenient.
        }
        
        const void* bytes = data->getBytesNoCopy();
        if (bytes) {
            memcpy(buffer, bytes, len);
            
            // Verify Signature (0x55 0xAA)
            uint8_t* b = (uint8_t*)buffer;
            if (b[0] == 0x55 && b[1] == 0xAA) {
                DBGLOG(AMD5K, "AMD5000Controller::readEfiRom(): Valid Signature Found");
                return true;
            } else {
                DBGLOG(AMD5K, "AMD5000Controller::readEfiRom(): Invalid Signature (0x%02X 0x%02X)", b[0], b[1]);
            }
        }
    }
    
    return false;
}

bool AMD5000::readVramRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(AMD5K, "AMD5000Controller::readVramRom(): called");

    if (orgReadVramRom) {
        bool result = FunctionCast(readVramRom, orgReadVramRom)(that, buffer, size);
        DBGLOG(AMD5K, "AMD5000Controller::readVramRom(): result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(AMD5K, "AMD5000Controller::readVramRom(): failed to call via trampoline!");
        return false;
    }

    return false;
}

bool AMD5000::readPciRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(AMD5K, "AMD5000Controller::readPciRom(): called");

    if (orgReadPciRom) {
        bool result = FunctionCast(readPciRom, orgReadPciRom)(that, buffer, size);
        DBGLOG(AMD5K, "AMD5000Controller::readPciRom(): result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(AMD5K, "AMD5000Controller::readPciRom(): failed to call via trampoline!");
        return false;
    }

    return false;
}

bool AMD5000::readRegRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(AMD5K, "AMD5000Controller::readRegRom(): called");
    
    if (orgReadRegRom) {
        bool result = FunctionCast(readRegRom, orgReadRegRom)(that, buffer, size);
        DBGLOG(AMD5K, "AMD5000Controller::readRegRom(): result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(AMD5K, "AMD5000Controller::readRegRom(): failed to call via trampoline!");
        return false;
    }

    return false;
}

int AMD5000::readATOMBIOS(void *that) {
    DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): called");

    // Access the BIOS storage structure pointer stored at 0x530
    BiosContainer* biosInfo = getMember<BiosContainer*>(that, 0x530);
    
    if (!biosInfo) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Error - BiosInfo struct is null");
        return kIOReturnError;
    }

    // Allocate 64KB buffer if needed
    if (biosInfo->buffer == nullptr) {
        biosInfo->buffer = IOMalloc(0x10000);
        biosInfo->size = 0x10000;
    }

    void* buffer = biosInfo->buffer;
    uint32_t size = biosInfo->size;

    if (!buffer) return kIOReturnNoMemory;

    bool success = false;

    // Try standard readers
    if (readEfiRom(that, buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from EFI ROM");
        success = true;
    } else if (readVramRom(that, buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from VRAM ROM");
        success = true;
    } else if (readPciRom(that, buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from PCI ROM");
        success = true;
    } else if (readRegRom(that, buffer, size)) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Loaded BIOS from Registry ROM");
        success = true;
    }

    // Fallback: Check for ATY,bin_image if standard readers failed
    // This mimics the original driver behavior at loc_17943
    if (!success) {
        DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Standard readers failed. Checking for ATY,bin_image fallback...");
        
        // Use offset 0x190 to get the provider (IOPCIDevice)
        IOService* provider = getMember<IOService*>(that, 0x190);
        
        if (provider) {
            OSObject* prop = provider->getProperty("ATY,bin_image");
            OSData* data = OSDynamicCast(OSData, prop);
            
            if (data && data->getLength() > 0) {
                size_t copySize = (data->getLength() > size) ? size : data->getLength();
                memcpy(buffer, data->getBytesNoCopy(), copySize);
                
                DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(Fallback): Loaded BIOS from ATY,bin_image (Size: %lu)", copySize);
                success = true;
            } else {
                DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(Fallback): ATY,bin_image not found on provider.");
            }
        } else {
            DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(Fallback): Provider at 0x190 is null.");
        }
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

    // Success - Propagate property if needed
    IOService* provider = getMember<IOService*>(that, 0x190);
    if (provider && !provider->getProperty("ATY,bin_image")) {
        OSData* data = OSData::withBytes(buffer, size);
        if (data) {
            provider->setProperty("ATY,bin_image", data);
            data->release();
            DBGLOG(AMD5K, "AMD5000Controller::readATOMBIOS(): Created ATY,bin_image property on provider.");
        }
    }
    
    return getBIOSInfo(that);
}

int AMD5000::getBIOSInfo(void *that) {
    BiosContainer* biosInfo = getMember<BiosContainer*>(that, 0x530);
    if (!biosInfo || !biosInfo->buffer) return kIOReturnNotFound;

    uint8_t* rom = (uint8_t*)biosInfo->buffer;
    
    // Offset 0x649 is the version string buffer
    char* versionBuf = (char*)((uintptr_t)that + 0x649);
    strncpy(versionBuf, "XXX-XXXXX-XXX", 0x20);

    // Header offset
    uint16_t headerOffset = rom[0x48] | (rom[0x49] << 8);
    
    // Safety check
    if (headerOffset + 4 > 0xFFFF) return 0xe00002c7;

    uint32_t signature = *reinterpret_cast<uint32_t*>(rom + headerOffset + 4);
    
    if (signature == 0x4D4F5441) { // 'ATOM'
        // Standard logic: Header + 0x6e
        uint8_t strOffset = rom[headerOffset + 0x6e];

        // Some PC VBIOSes put the SKU string elsewhere.
        bool foundSku = false;
        
        // Quick scan in the first 512 bytes for "113-"
        // Will be improved as more VBIOS SKU data is gathered
        for (int i = 0; i < 512; i++) {
            if (rom[i] == '1' && rom[i+1] == '1' && rom[i+2] == '3' && rom[i+3] == '-') {
                strncpy(versionBuf, (char*)(rom + i), 0x20);
                DBGLOG(AMD5K, "VBIOS SKU Found (Scan): %s", versionBuf);
                foundSku = true;
                break;
            }
        }

        // If scan failed, use the standard offset (even if it's the long description)
        if (!foundSku && strOffset != 0) {
            strncpy(versionBuf, (char*)(rom + strOffset), 0x20);
            DBGLOG(AMD5K, "VBIOS Version (Standard): %s", versionBuf);
        }
        
        // Always return success if we have a valid AtomBIOS
        getMember<uint8_t>(that, 0x668) = 0;
        return kIOReturnSuccess;
    }

    getMember<uint8_t>(that, 0x668) = 0;
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

