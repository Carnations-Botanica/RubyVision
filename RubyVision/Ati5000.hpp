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
    // Trampolines
    static mach_vm_address_t orgGetDeviceId;
	
    // Reimplementations
    static uint32_t getDeviceId(void *that);
};

#endif /* Ati5000_h */
