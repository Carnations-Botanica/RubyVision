//
//  Ati5000.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/28/25.
//

#ifndef Ati5000_h
#define Ati5000_h

// Base Imports
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_api.hpp>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSData.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOService.h>
#include <mach/vm_map.h>
#include <IOKit/IOLib.h>
#include <sys/proc.h>
#include "kern_start.hpp"

// Logging Defs
#define ATI5K "ATI5K"

// Root/Main ATI5000 Class
class Ati5000 {
public:
    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    // Trampolines for hooks
    static mach_vm_address_t orgGetDeviceId;
    static mach_vm_address_t orgReadATOMBIOS;
    static mach_vm_address_t orgReadEfiRom;
    static mach_vm_address_t orgReadVramRom;
    static mach_vm_address_t orgReadPciRom;
    static mach_vm_address_t orgReadRegRom;
    static mach_vm_address_t orgGetBIOSInfo;

    // External Trampolines (Parent class methods needed for reimplementations)
    static mach_vm_address_t orgMapPCISubRange; // ATIController::mapPCISubRange

    // Hook Implementations
    static uint32_t getDeviceId(void *that);
    static int readATOMBIOS(void *that);
    static bool readEfiRom(void *that, void *buffer, uint32_t size);
    static bool readVramRom(void *that, void *buffer, uint32_t size);
    static bool readPciRom(void *that, void *buffer, uint32_t size);
    static bool readRegRom(void *that, void *buffer, uint32_t size);
    static int getBIOSInfo(void *that);
    
    // Helper to access raw member offsets
    template <typename T>
    static T& getMember(void* that, size_t offset) {
        return *reinterpret_cast<T*>((uintptr_t)that + offset);
    }
    
};

#endif /* Ati5000_h */
