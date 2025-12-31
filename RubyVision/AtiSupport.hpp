//
//  AtiSupport.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/29/25.
//

#ifndef AtiSupport_h
#define AtiSupport_h

// Base Includes
#include <Headers/kern_patcher.hpp>
#include <libkern/c++/OSObject.h>
#include <Headers/kern_util.hpp>
#include <Headers/kern_api.hpp>
#include "kern_start.hpp"

// Logging Defs
#define ATISUP "ATISUP"

class AtiSupport {
public:
    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    // Trampolines
    static mach_vm_address_t orgGetAtomObjectTableForType;

    // Reimplementations
    
    /**
     * Hook for __ZN20AtiAtomBiosUtilities25getAtomObjectTableForTypeEhRh
     * Signature: void* AtiAtomBiosUtilities::getAtomObjectTableForType(unsigned char type, unsigned char &revision)
     */
    static void *getAtomObjectTableForType(void *that, uint8_t type, uint8_t &revision);
};

#endif /* AtiSupport_h */