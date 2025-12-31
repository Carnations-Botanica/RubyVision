//
//  AtiDbg2000.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/26/25.
//

#ifndef AtiDbg2000_h
#define AtiDbg2000_h

#include <Headers/kern_patcher.hpp>

class AtiDbg2000 {
public:

    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
	static mach_vm_address_t orgGetChipID;
	static mach_vm_address_t orgKlogInitLogLevels;
	static mach_vm_address_t orgWindowModeToAtiFormat;
	static mach_vm_address_t logLevelMasksAddr;
	static size_t logLevelMasksCount;
	static uint32_t getChipID(void *that);
	static void klogInitLogLevels();
	static uint32_t windowModeToAtiFormat(uint32_t mode);
	
};

#endif /* AtiDbg2000_h */
