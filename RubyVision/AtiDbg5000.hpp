//
//  AtiDbg5000.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/28/25.
//

#ifndef AtiDbg5000_h
#define AtiDbg5000_h

#include <Headers/kern_patcher.hpp>
#include <libkern/c++/OSObject.h>

class AtiDbg5000 {
public:
    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    static mach_vm_address_t orgGetDeviceId;
    static mach_vm_address_t orgReadATOMBIOS;
    static mach_vm_address_t orgReadEfiRom;
    static mach_vm_address_t orgReadVramRom;
    static mach_vm_address_t orgReadPciRom;
    static mach_vm_address_t orgReadRegRom;
    static mach_vm_address_t orgGetBIOSInfo;
    static mach_vm_address_t orgInitializeProjectInfo;

    static uint32_t getDeviceId(void *that);
    static int readATOMBIOS(void *that);
    static int readEfiRom(void *that, uint8_t *buffer, uint32_t length);
    static int readVramRom(void *that, uint8_t *buffer, uint32_t length);
    static int readPciRom(void *that, uint8_t *buffer, uint32_t length);
    static int readRegRom(void *that, uint8_t *buffer, uint32_t length);
    static int getBIOSInfo(void *that);
    static void initializeProjectInfo(void *that);

};

#endif /* AtiDbg5000_h */