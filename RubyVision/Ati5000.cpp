//
//  Ati5000.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/28/25.
//

#include "Ati5000.hpp"

// Initialize trampolines
mach_vm_address_t Ati5000::orgGetDeviceId = 0;
mach_vm_address_t Ati5000::orgReadATOMBIOS = 0;
mach_vm_address_t Ati5000::orgReadEfiRom = 0;
mach_vm_address_t Ati5000::orgReadVramRom = 0;
mach_vm_address_t Ati5000::orgReadPciRom = 0;
mach_vm_address_t Ati5000::orgReadRegRom = 0;
mach_vm_address_t Ati5000::orgGetBIOSInfo = 0;
mach_vm_address_t Ati5000::orgMapPCISubRange = 0;

// Internal structure helper based on ASM
struct BiosContainer {
    void* buffer;      // offset 0
    uint32_t size;     // offset 8
    // padding likely exists between 0 and 8 on 64-bit
};

// Define paths for targeted kexts
static const char *pathATI5000Controller[] { "/System/Library/Extensions/ATI5000Controller.kext/Contents/MacOS/ATI5000Controller" };
static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.ATI5000Controller", pathATI5000Controller, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

// Main Ati5000 Routine
void Ati5000::init() {
    DBGLOG(ATI5K, "Initializing ATI5000Controller hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

// Main 5000Controller hook routine
void Ati5000::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    // Identify which kext from our list triggered this callback
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
    if (strcmp(kextList[kextIndex].id, "com.apple.kext.ATI5000Controller") != 0) {
        return;
    }

    DBGLOG(ATI5K, "Found %s (of Global Index %lu) - Hooking...", kextList[kextIndex].id, index);

    KernelPatcher::RouteRequest requests[] {
        { "__ZN17ATI5000Controller11getDeviceIdEv", getDeviceId, orgGetDeviceId },
        { "__ZN17ATI5000Controller12readATOMBIOSEv", readATOMBIOS, orgReadATOMBIOS },
        { "__ZN17ATI5000Controller11getBIOSInfoEv", getBIOSInfo, orgGetBIOSInfo },
        { "__ZN17ATI5000Controller10readEfiRomEPhj", readEfiRom, orgReadEfiRom },
        { "__ZN17ATI5000Controller11readVramRomEPhj", readVramRom, orgReadVramRom },
        { "__ZN17ATI5000Controller10readPciRomEPhj", readPciRom, orgReadPciRom },
        { "__ZN17ATI5000Controller10readRegRomEPhj", readRegRom, orgReadRegRom },
        { "__ZN13ATIController14mapPCISubRangeE13PCI_REG_INDEXyy", nullptr, orgMapPCISubRange }
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG(ATI5K, "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG(ATI5K, "Successfully hooked functions for Index %lu.", index);
    }

}

uint32_t Ati5000::getDeviceId(void *that) {
    uint32_t deviceId = 0;
    char procName[64] = {0};
    proc_selfname(procName, sizeof(procName));
    int pid = proc_selfpid();
    DBGLOG(ATI5K, "[%s:%d] ATI5000Controller::getDeviceId() called.", procName, pid);
    
    // Get the real Device ID from the hardware/original function
    if (orgGetDeviceId) {
        deviceId = FunctionCast(getDeviceId, orgGetDeviceId)(that);
        DBGLOG(ATI5K, "[%s:%d] ATI5000Controller::getDeviceId() got Device ID: 0x%04X", procName, pid, deviceId);
    } else {
        deviceId = 0x0000;
        DBGLOG(ATI5K, "Ati5000::getDeviceId() failed to call via trampoline!");
        return deviceId;
    }

    // Spoof injected devices to architecture match
    // 68F9 -> 68E0
    // XXXX -> XXXX
    uint32_t originalId = deviceId;
    if ((deviceId & 0xFFFF) == 0x68F9) {
        deviceId = (deviceId & 0xFFFF0000) | 0x68E0;
    }

    // We log out if a card supported by RV is being actively spoofed
    if ((originalId & 0xFFFF) == 0x68F9) {
        DBGLOG(ATI5K, "    Real ID: 0x%04X -> Spoofed ID: 0x%04X", originalId, deviceId);
    }

    return deviceId;
}

bool Ati5000::readEfiRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(ATI5K, "readEfiRom called (Size: %d)", size);
    
    // Offset 0x32 is likely the IOPCIDevice* provider
    IOPCIDevice* device = getMember<IOPCIDevice*>(that, 0x32);
    if (!device) return false;

    // Check for "ATY,bin_image" on the provider
    OSObject* prop = device->getProperty("ATY,bin_image");
    OSData* data = OSDynamicCast(OSData, prop);
    
    if (data) {
        uint32_t len = data->getLength();
        DBGLOG(ATI5K, "Found ATY,bin_image in EFI (Len: %d)", len);
        
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
                DBGLOG(ATI5K, "readEfiRom: Valid Signature Found");
                return true;
            } else {
                DBGLOG(ATI5K, "readEfiRom: Invalid Signature (0x%02X 0x%02X)", b[0], b[1]);
            }
        }
    }
    
    return false;
}

bool Ati5000::readVramRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(ATI5K, "Ati5000::readVramRom() called");

    // Requires mapPCISubRange logic.
    // For now, let's call the original implementation but log the result.
    // Reimplementing mapping logic entirely requires exact IOMemoryMap offsets.
    
    if (orgReadVramRom) {
        bool result = FunctionCast(readVramRom, orgReadVramRom)(that, buffer, size);
        DBGLOG(ATI5K, "Ati5000::readVramRom() result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(ATI5K, "Ati5000::readVramRom() failed to call via trampoline!");
        return false;
    }

    return false;
}

bool Ati5000::readPciRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(ATI5K, "Ati5000::readPciRom() called");

    if (orgReadPciRom) {
        bool result = FunctionCast(readPciRom, orgReadPciRom)(that, buffer, size);
        DBGLOG(ATI5K, "Ati5000::readPciRom() result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(ATI5K, "Ati5000::readPciRom() failed to call via trampoline!");
        return false;
    }

    return false;
}

bool Ati5000::readRegRom(void *that, void *buffer, uint32_t size) {
    DBGLOG(ATI5K, "Ati5000::readRegRom() called");
    
    // This uses register index 0xA8 and data 0xAC.
    // We can try to emulate this or call original.
    // Calling original allows us to spy on it for now.
    
    if (orgReadRegRom) {
        bool result = FunctionCast(readRegRom, orgReadRegRom)(that, buffer, size);
        DBGLOG(ATI5K, "Ati5000::readRegRom() result: %s", result ? "Success" : "Fail");
        return result;
    } else {
        DBGLOG(ATI5K, "Ati5000::readRegRom() failed to call via trampoline!");
        return false;
    }

    return false;
}

int Ati5000::readATOMBIOS(void *that) {
    DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): called");

    // Access the BIOS storage structure pointer stored at 0x530
    BiosContainer* biosInfo = getMember<BiosContainer*>(that, 0x530);
    
    if (!biosInfo) {
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Error - BiosInfo struct is null");
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
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Loaded BIOS from EFI ROM");
        success = true;
    } else if (readVramRom(that, buffer, size)) {
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Loaded BIOS from VRAM ROM");
        success = true;
    } else if (readPciRom(that, buffer, size)) {
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Loaded BIOS from PCI ROM");
        success = true;
    } else if (readRegRom(that, buffer, size)) {
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Loaded BIOS from Registry ROM");
        success = true;
    }

    // Fallback: Check for ATY,bin_image if standard readers failed
    // This mimics the original driver behavior at loc_17943
    if (!success) {
        DBGLOG(ATI5K, "Standard readers failed. Checking for ATY,bin_image fallback...");
        
        // Use offset 0x190 to get the provider (IOPCIDevice)
        IOService* provider = getMember<IOService*>(that, 0x190);
        
        if (provider) {
            OSObject* prop = provider->getProperty("ATY,bin_image");
            OSData* data = OSDynamicCast(OSData, prop);
            
            if (data && data->getLength() > 0) {
                size_t copySize = (data->getLength() > size) ? size : data->getLength();
                memcpy(buffer, data->getBytesNoCopy(), copySize);
                
                DBGLOG(ATI5K, "Ati5000::readATOMBIOS(Fallback): Loaded BIOS from ATY,bin_image (Size: %lu)", copySize);
                success = true;
            } else {
                DBGLOG(ATI5K, "Ati5000::readATOMBIOS(Fallback): ATY,bin_image not found on provider.");
            }
        } else {
            DBGLOG(ATI5K, "Ati5000::readATOMBIOS(Fallback): Provider at 0x190 is null.");
        }
    }

    // Failure handling
    if (!success) {
        DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Failed to load BIOS from any source!");
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
            DBGLOG(ATI5K, "Ati5000::readATOMBIOS(): Created ATY,bin_image property on provider.");
        }
    }
    
    return getBIOSInfo(that);
}

int Ati5000::getBIOSInfo(void *that) {
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
                DBGLOG(ATI5K, "VBIOS SKU Found (Scan): %s", versionBuf);
                foundSku = true;
                break;
            }
        }

        // If scan failed, use the standard offset (even if it's the long description)
        if (!foundSku && strOffset != 0) {
            strncpy(versionBuf, (char*)(rom + strOffset), 0x20);
            DBGLOG(ATI5K, "VBIOS Version (Standard): %s", versionBuf);
        }
        
        // Always return success if we have a valid AtomBIOS
        getMember<uint8_t>(that, 0x668) = 0;
        return kIOReturnSuccess;
    }

    getMember<uint8_t>(that, 0x668) = 0;
    return 0xe00002c7;
}
