//
//  kern_start.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/24/25.
//

#include "kern_start.hpp"
#include "AtiDbg2000.hpp"
#include "AtiDbg5000.hpp"
#include "AtiDbgSupport.hpp"

static RUBY rubyInstance;
RUBY *RUBY::callbackRUBY;

// Define and init various static member variables
int RUBY::darwinMajor = 0;
int RUBY::darwinMinor = 0;
mach_vm_address_t RUBY::orgGetProperty = 0;

// To only be modified by CarnationsInternal, to display various Internal logs and headers
const bool RUBY::IS_INTERNAL = false; // MUST CHANCE THIS TO FALSE BEFORE CREATING COMMITS

// Main RUBY Routine function
void RUBY::init() {

    callbackRUBY = this;
	RUBY::darwinMajor = getKernelVersion();
	RUBY::darwinMinor = getKernelMinorVersion();
    const char* rubyVersionNumber = RUBY_VERSION;
    DBGLOG(MODULE_INIT, "Hello World from RubyVision!");
    DBGLOG(MODULE_INFO, "Current Build Version running: %s", rubyVersionNumber);
    DBGLOG(MODULE_INFO, "Copyright © 2025 Carnations Botanica. All rights reserved.");
    if (RUBY::darwinMajor > 0) {
        DBGLOG(MODULE_INFO, "Current Darwin Kernel version: %d.%d", RUBY::darwinMajor, RUBY::darwinMinor);
    } else {
        DBGLOG(MODULE_ERROR, "WARNING: Failed to retrieve Darwin Kernel version.");
    }

    // Internal Header BEGIN
    if (RUBY::IS_INTERNAL) {
        DBGLOG(MODULE_WARN, "");
        DBGLOG(MODULE_WARN, "==================================================================");
		DBGLOG(MODULE_WARN, "This build of %s is for CarnationsInternal usage only!", MODULE_LONG);
        DBGLOG(MODULE_WARN, "If you received a copy of this binary as a tester, DO NOT SHARE.");
        DBGLOG(MODULE_WARN, "==================================================================");
        DBGLOG(MODULE_WARN, "");
    }
    // Internal Header END
	
    // Begin based on kernel version detected.
	// This is messy because internally, we're debugging 
    // and creating work for each version and gpu combo
    // independently, and one at a time...
	if (RUBY::darwinMajor >= KernelVersion::Tahoe) {
        DBGLOG(MODULE_INIT, "Detected macOS Tahoe (16.x) or newer.");

    } else if (RUBY::darwinMajor >= KernelVersion::Sequoia) {
        DBGLOG(MODULE_INIT, "Detected macOS Sequoia (15.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Sonoma) {
        DBGLOG(MODULE_INIT, "Detected macOS Sonoma (14.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Ventura) {
        DBGLOG(MODULE_INIT, "Detected macOS Ventura (13.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Monterey) {
        DBGLOG(MODULE_INIT, "Detected macOS Monterey (12.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::BigSur) {
        DBGLOG(MODULE_INIT, "Detected macOS Big Sur (11.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Catalina) {
        DBGLOG(MODULE_INIT, "Detected macOS Catalina (10.15.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Mojave) {
        DBGLOG(MODULE_INIT, "Detected macOS Mojave (10.14.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::HighSierra) {
        DBGLOG(MODULE_INIT, "Detected macOS High Sierra (10.13.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Sierra) {
        DBGLOG(MODULE_INIT, "Detected macOS Sierra (10.12.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::ElCapitan) {
        DBGLOG(MODULE_INIT, "Detected OS X El Capitan (10.11.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Yosemite) {
        DBGLOG(MODULE_INIT, "Detected OS X Yosemite (10.10.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::Mavericks) {
        DBGLOG(MODULE_INIT, "Detected OS X Mavericks (10.9.x).");

    } else if (RUBY::darwinMajor >= KernelVersion::MountainLion) {
        DBGLOG(MODULE_INIT, "Detected OS X Mountain Lion (10.8.x).");
        lilu.onPatcherLoad(processKernel, nullptr);
        AtiDbgSupport::init();
		AtiDbg5000::init();
        
    } else if (RUBY::darwinMajor >= KernelVersion::Lion) {
        DBGLOG(MODULE_INIT, "Detected OS X Lion (10.7.x).");
		DBGLOG(MODULE_INIT, "Nothing to do!");

    } else if (RUBY::darwinMajor >= KernelVersion::SnowLeopard) {
        DBGLOG(MODULE_INIT, "Detected OS X Snow Leopard (10.6.x).");
		DBGLOG(MODULE_INIT, "Nothing to do!");

    } else if (RUBY::darwinMajor >= KernelVersion::Leopard) {
        DBGLOG(MODULE_INIT, "Detected OS X Leopard (10.5.x).");
        AtiDbg2000::init();

    } else if (RUBY::darwinMajor >= KernelVersion::Tiger) {
        DBGLOG(MODULE_INIT, "Detected OS X Tiger (10.4.x).");
        AtiDbg2000::init();

    } else {
        // How the helly? you're on PPC or some shit?
        DBGLOG(MODULE_ERROR, "Detected an unsupported version of OS X / macOS.");
        panic(MODULE_LONG, "Detected an unsupported version of OS X / macOS.");
    }

	// todo
}

// Global kernel hooks routine
void RUBY::processKernel(void *user, KernelPatcher &patcher) {
    DBGLOG(MODULE_SHORT, "Hooking various kernel functions for better debugging...");

    KernelPatcher::RouteRequest kernelRequests[] {
        { "__ZNK15IORegistryEntry11getPropertyEPKc", getProperty, orgGetProperty }
    };

    if (!patcher.routeMultiple(KernelPatcher::KernelID, kernelRequests, arrsize(kernelRequests))) {
         DBGLOG(MODULE_SHORT, "Failed to hook various kernel functions.");
    } else {
         DBGLOG(MODULE_SHORT, "Successfully hooked kernel functions.");
    }

}

// XNU getProperty, modified to DBGLOG what was requested,
// and what will be sent as the response. Includes filter.
OSObject* RUBY::getProperty(void *that, const char *key) {
    OSObject *result = nullptr;

    // Call the original routine, have it finish the request
    if (orgGetProperty) {
        typedef OSObject* (*t_getProp)(void *, const char *);
        result = reinterpret_cast<t_getProp>(orgGetProperty)(that, key);
    }

    // Begin tomfoolery
    if (key && that) {
        bool skip = false;
        
        // If the object is irrelevant, we skip immediately without checking keys.
        IORegistryEntry *entry = static_cast<IORegistryEntry *>(that);
        const char *name = entry->getName();

        if (name) {
            static const char *skippedObjects[] = {
                "Lilu", // Don't debug ourselves looking for properties
                "com_apple_AppleFSCompression_AppleFSCompressionTypeDataless",
                "com_apple_AppleFSCompression_AppleFSCompressionTypeZlib",
                "CORSAIR GLAIVE RGB PRO Gaming Mouse",
                "XHCI Root Hub USB 2.0 Simulation",
                "IOPCIMessagedInterruptController",
                "AppleAPICInterruptController",
                "XHCI Root Hub SS Simulation",
                "CORSAIR iCUE Commander CORE",
                "IOHIDEventSystemUserClient",
                "AppleMCCSParameterHandler",
                "AppleSCSISubsystemGlobals",
                "IOHIDStackShotUserClient",
                "IONetworkStackUserClient",
                "AppleACPIEventController",
                "IOBluetoothSerialManager",
                "IOAHCIBlockStorageDevice",
                "IOBluetoothHCIController",
                "AppleACPIPlatformExpert",
                "AppleIntel8254XEthernet",
                "ACPI_SMC_PlatformPlugin",
                "AppleACPIInterruptLink",
                "AppleMCCSControlModule",
                "IOUserEthernetResource",
                "Apple USB Multiplexor",
                "IOGUIDPartitionScheme",
                "IOAudioToggleControl",
                "AppleUSBEthernetHost",
                "IOBlockStorageDriver",
                "EFI System Partition",
                "RootDomainUserClient",
                "IOUSBCompositeDriver",
                "IOHIDParamUserClient",
                "Dont_Steal_Mac_OS_X",
                "com_apple_BootCache",
                "IOEthernetInterface",
                "QEMU HARDDISK Media",
                "IOUSBUserClientInit",
                "AURA LED Controller",
                "AppleUSBAudioEngine",
                "AppleUSBAudioStream",
                "AppleAHCIDiskDriver",
                "IOAudioLevelControl",
                "AppleUSBAudioDevice",
                "IOHIDLibUserClient",
                "Keychron K10 Pro",
                "IOHDIXController",
                "IOKernelDebugger",
                "HyperX 7.1 Audio",
                "AppleUSBMergeNub",
                "IOHIDEventDriver",
                "AppleUSBEthernet",
                "IOMediaBSDClient",
                "AppleACPIButton",
                "AppleEFIRuntime",
                "IOPCI2PCIBridge",
                "IOPCI2PCIBridge",
                "GenericUSBXHCI",
                "IOUSBInterface",
                "IONetworkStack",
                "IOHIDInterface",
                "IOPMrootDomain",
                "IOUSBHIDDriver",
                "IOHIDPointing",
                "IOHIDKeyboard",
                "IOHIDResource",
                "Mountain Lion",
                "IOHIDConsumer",
                "power-button",
                "IOAHCIDevice",
                "AppleACPICPU",
                "AppleACPIPCI",
                "IORootParent",
                "IOUSBDevice",
                "IOResources",
                "Recovery HD",
                "AppleUSBHub",
                "IOHIDSystem",
                "Macintosh 1",
                "Macintosh 2",
                "AppleSMBIOS",
                "Untitled 1",
                "USB2.0 Hub",
                "AppleHPET",
                "MacPro3,1",
                "Macintosh",
                "AppleACPI",
                "AppleAHCI",
                "AppleSMC",
                "platform",
                "AppleRTC",
                "io-apic",
                "iPhone",
                "chosen",
                "IOKDP",
                "Root",
                "GSIG",
                "GSIA",
                "GSIH",
                "GSIF",
                "CPUS",
                "HPET",
                "DRAC",
                "PRT0",
                "PRT1",
                "PRT2",
                "PRT3",
                "COM1",
                "PRT4",
                "C000",
                "C001",
                "C002",
                "C003",
                "C004",
                "bios",
                "cpus",
                "RTC",
                "KBD",
                "MOU",
                "PTP",
                "S0D",
                "S0B",
                "S0C",
                "SFB",
                "SFA",
                "en0",
                "en1",
                "S01", "S08", "S09",
            };
            
            for (size_t i = 0; i < arrsize(skippedObjects); i++) {
                if (strcmp(name, skippedObjects[i]) == 0) {
                    skip = true;
                    break;
                }
            }
        }

        // Only check keys if the object wasn't already skipped.
        if (!skip) {
            static const char *skippedKeys[] = {
                "AbsoluteAxisBoundsRemovalPercentage",
                "HIDDisallowRemappingOfPrimaryClick",
                "New Interface Detected Action",
                "NetworkConfigurationOverrides",
                "IOPMUnattendedWakePowerState",
                "HIDPointerAccelerationType",
                "IOUserClientSharedInstance",
                "HIDScrollAccelerationType",
                "Protocol Characteristics",
                "IOMaximumBlockCountWrite",
                "IOMaximumBlockCountRead",
                "IOMaximumByteCountWrite",
                "IOMaximumByteCountRead",
                "Device Characteristics",
                "HIDTrackingAccelCurves",
                "efilogin-unlock-ident",
                "Tunnel Endpoint GUID",
                "efilogin-user-ident",
                "HiddenConfiguration",
                "CFBundleIdentifier",
                "DevicePPPOverrides",
                "IOMediumDictionary",
                "DevicePPPOverride",
                "eject-upon-logout",
                "IOUserClientClass",
                "IOGeneralInterest",
                "IOStorageFeatures",
                "IOCommandPoolSize",
                "IOCommandPoolSize",
                "USB Product Name",
                "HIDVirtualDevice",
                "bootroot-active",
                "als-lgp-version",
                "IOMatchCategory",
                "bInterfaceClass",
                "IOProviderClass",
                "HIDAccelCurves",
                "IOPCIResourced",
                "throttle-unit",
                "MTEventSource",
                "InterfaceRole",
                "od-media-type",
                "autodiskmount",
                "bDeviceClass",
                "Content Hint",
                "Product Name",
                "IOProbeScore",
                "IOBootDevice",
                "ExtendedData",
                "IOMediaIcon",
                "owner-mode",
                "IOGVACodec",
                "layout-id",
                "owner-gid",
                "owner-uid",
                "BSD Name",
                "BSD Unit",
                "Writable",
                "IOClass",
                "IOUnit",
                "IOName",
                "Whole",
                "GUID",
                "Leaf",
            };
            
            for (size_t i = 0; i < arrsize(skippedKeys); i++) {
                if (strcmp(key, skippedKeys[i]) == 0) {
                    skip = true;
                    break;
                }
            }
        }
        
        // Log anything that survived
        if (!skip) {
            DBGLOG(ATIDBG, "[%s] getProperty('%s') -> %s", name ? name : "Unknown", key, result ? "Found" : "NULL");
            
            if (result) {
                // OSString
                OSString *strVal = OSDynamicCast(OSString, result);
                if (strVal) {
                    DBGLOG(ATIDBG, "    Value (String): %s", strVal->getCStringNoCopy());
                }
                
                // OSData
                OSData *dataVal = OSDynamicCast(OSData, result);
                if (dataVal) {
                    uint32_t len = dataVal->getLength();
                    const uint8_t *bytes = (const uint8_t *)dataVal->getBytesNoCopy();
                    if (len > 0) {
                        DBGLOG(ATIDBG, "    Value (OSData, Len=%d): %02X %02X %02X %02X...", 
                               len, bytes[0], (len>1)?bytes[1]:0, (len>2)?bytes[2]:0, (len>3)?bytes[3]:0);
                        if (len < 64 && bytes[0] >= 0x20 && bytes[0] <= 0x7E) {
                             DBGLOG(ATIDBG, "    Value (OSData as String): %.*s", len, bytes);
                        }
                    }
                }
                
                // OSBoolean
                OSBoolean *boolVal = OSDynamicCast(OSBoolean, result);
                if (boolVal) {
                    DBGLOG(ATIDBG, "    Value (OSBoolean): %s", boolVal->isTrue() ? "True" : "False");
                }
                
                // OSSymbol
                OSSymbol *symVal = OSDynamicCast(OSSymbol, result);
                if (symVal) {
                    DBGLOG(ATIDBG, "    Value (OSSymbol): %s", symVal->getCStringNoCopy());
                }

                // OSNumber
                OSNumber *numVal = OSDynamicCast(OSNumber, result);
                if (numVal) {
                    DBGLOG(ATIDBG, "    Value (OSNumber): %llu", numVal->unsigned64BitValue());
                }
            }
        }
    }
    
    return result;
}

// We use rubyState to determine RUBY behaviour
void RUBY::deinit() {
    DBGLOG(MODULE_ERROR, "This kernel extension cannot be disabled this way!");
    SYSLOG(MODULE_ERROR, "This kernel extension cannot be disabled this way!");
}

const char *bootargOff[] {
    "-rubyoff"
};

const char *bootargDebug[] {
    "-rubydbg"
};

const char *bootargBeta[] {
    "-rubybeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal |
    LiluAPI::AllowSafeMode |
    LiluAPI::AllowInstallerRecovery,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
	KernelVersion::Tiger,
	KernelVersion::Tahoe,
    []() {
        
        // Start the main RUBY routine
        rubyInstance.init();
        
    }
};
