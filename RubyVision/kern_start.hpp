//
//  kern_start.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/24/25.
//

#ifndef kern_start_h
#define kern_start_h

// Base Includes
#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IORegistryEntry.h>
#include <Headers/kern_nvram.hpp>
#include <libkern/c++/OSString.h>
#include <Headers/kern_util.hpp>
#include <Headers/kern_mach.hpp>
#include <Headers/kern_efi.hpp>
#include <mach/i386/vm_types.h>
#include <Headers/kern_api.hpp>
#include <libkern/libkern.h>
#include <IOKit/IOLib.h>
#include <sys/sysctl.h>
#include <i386/cpuid.h>

// Logging Defs
#define MODULE_INIT "INIT"
#define MODULE_SHORT "RUBY"
#define MODULE_LONG "RubyVision"
#define MODULE_ERROR "ERR"
#define MODULE_WARN "WARN"
#define MODULE_INFO "INFO"
#define MODULE_CUTE "\u2665"

#define ATIDBG "ATIDBG"
#define AMDDBG "AMDDBG"

// Root/Parent Class
class RUBY {
public:
    
    /**
     * Standard functions
     */
    void init();
    void deinit();
    
    /**
    * Publicly accessible internal build flag
    */
    static const bool IS_INTERNAL;
    
    /**
     * Must be populated by PHTM::init() before can read
     */
    static int darwinMajor;
    static int darwinMinor;
    
private:
    
    /**
     * Private self instance for callbacks
     */
    static RUBY *callbackRUBY;

    /**
     * Included procKern function for AtiDbg'ing
     */
    static void processKernel(void *user, KernelPatcher &patcher);

    // AtiDbg has been consumed by the main routine, ensuring all versions
    // are able to easily debug ATI/AMD related functions on the system.
    // Trampolines
    static mach_vm_address_t orgGetProperty;

    // Reimplementations
    static OSObject* getProperty(void *that, const char *key);
    
};

#endif /* kern_start_h */

#ifndef RUBY_VERSION /*RUBY_VERSION Macro */
#define RUBY_VERSION "Unknown"

#endif /* RUBY_VERSION Macro */
