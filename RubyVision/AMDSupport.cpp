//
//  AMDSupport.cpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/31/25.
//

#include "AMDSupport.hpp"

struct ConnectorInfo {
    uint32_t mac_type;       // 0x00
    uint32_t flags;          // 0x04
    uint32_t priority;       // 0x08
    uint16_t sense_id;       // 0x0C
    uint16_t atom_object_id; // 0x0E
};

const char* AMDSupport::getConnectorTypeName(uint32_t type) {
    switch (type) {
        case ConnectorLVDS:       return "LVDS";
        case ConnectorDigitalDVI: return "DVI-D";
        case ConnectorSVID:       return "S-Video";
        case ConnectorVGA:        return "VGA";
        case ConnectorDP:         return "DisplayPort";
        case ConnectorHDMI:       return "HDMI";
        case ConnectorAnalogDVI:  return "DVI-A";
        default:                  return "Unknown";
    }
}

// Initialize static members
mach_vm_address_t AMDSupport::orgControllerStart = 0;
mach_vm_address_t AMDSupport::orgControllerProbe = 0;
mach_vm_address_t AMDSupport::orgGetConnectorsInfo = 0;
mach_vm_address_t AMDSupport::orgGetAtomObjectTable = 0;
mach_vm_address_t AMDSupport::orgGetAtomObjectTableForType = 0;
mach_vm_address_t AMDSupport::orgCheckDDCConnections = 0;
mach_vm_address_t AMDSupport::orgDoesSupportsProject = 0;
mach_vm_address_t AMDSupport::orgInitializeProjectInfo = 0;
mach_vm_address_t AMDSupport::orgInitializeProjectInfoString = 0;

// Define paths
static const char *pathAMDSupport[] { "/System/Library/Extensions/ATISupport.kext/Contents/MacOS/AMDSupport" };

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.kext.AMDSupport", pathAMDSupport, 1, {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

void AMDSupport::init() {
    DBGLOG(AMDSUP, "Initializing AMDSupport hooks...");
    lilu.onKextLoadForce(kextList, arrsize(kextList), processKext, nullptr);
}

void AMDSupport::processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
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

    if (strcmp(kextList[kextIndex].id, "com.apple.kext.AMDSupport") != 0) {
        return;
    }
    
    DBGLOG(AMDSUP, "Found %s (of Global Index %lu) - Hooking...", kextList[kextIndex].id, index);

    KernelPatcher::RouteRequest requests[] {
        { "__ZN13ATIController5startEP9IOService", controllerStart, orgControllerStart },
        { "__ZN13ATIController5probeEP9IOServicePi", controllerProbe, orgControllerProbe },
        { "__ZN13ATIController19checkDDCConnectionsEP12ATIConnectorjP4Edid", checkDDCConnections, orgCheckDDCConnections },
        { "__ZN13ATIController19doesSupportsProjectEP8OSString", doesSupportsProject, orgDoesSupportsProject },
        { "__ZN13ATIController21initializeProjectInfoEv", initializeProjectInfo, orgInitializeProjectInfo },
        { "__ZN13ATIController21initializeProjectInfoEP8OSString", initializeProjectInfoString, orgInitializeProjectInfoString },
        { "__ZN20AtiAtomBiosUtilities18getAtomObjectTableEv", getAtomObjectTable, orgGetAtomObjectTable },
        { "__ZN20AtiAtomBiosUtilities25getAtomObjectTableForTypeEhRh", getAtomObjectTableForType, orgGetAtomObjectTableForType },
        { "__ZN23AtiAtomBiosDceInterface17getConnectorsInfoEP13ConnectorInfoRh", getConnectorsInfo, orgGetConnectorsInfo }
    };

    if (!patcher.routeMultiple(index, requests, arrsize(requests), address, size)) {
        DBGLOG(AMDSUP, "Failed to hook functions for Index %lu (Error: %d).", index, patcher.getError());
    } else {
        DBGLOG(AMDSUP, "Successfully hooked functions for Index %lu.", index);
    }
}

// Helper to dbgout the connectors
void AMDSupport::dumpRawConnectors(void *connectorInfo, int count) {
    auto *connectors = static_cast<LegacyConnector *>(connectorInfo);
    DBGLOG(AMDSUP, "Raw Connectors Dump (Count: %d)", count);

    for (int i = 0; i < count; i++) {
        LegacyConnector *c = &connectors[i];
        DBGLOG(AMDSUP, "  [%d] Type:0x%X Sense:0x%X Prio:%d Enc:%d Tx:0x%X",
               i, c->type, c->sense, c->priority, c->encoder, c->transmitter);
    }
}

// Helper to dbgout the connector table gotten
void AMDSupport::dumpConnectorTable(void *tableData) {
    DBGLOG(AMDSUP, "ATIController::dumpConnectorTable(): called");

    // If count looks wrong, dump raw bytes
    auto *table = static_cast<ATOM_CONNECTOR_OBJECT_TABLE *>(tableData);
    if (table->ucConnectorCount > 16) {
        DBGLOG(AMDSUP, "    (Invalid Table? Raw Header: %02X %02X %02X %02X)", 
               ((uint8_t*)tableData)[0], ((uint8_t*)tableData)[1], ((uint8_t*)tableData)[2], ((uint8_t*)tableData)[3]);
        return;
    }

    DBGLOG(AMDSUP, "    Table Header: Size=%d, Rev=%d.%d", 
           table->sHeader.usStructureSize, table->sHeader.ucTableContentRevision, table->sHeader.ucTableFormatRevision);
    DBGLOG(AMDSUP, "    Connector Count: %d", table->ucConnectorCount);

    for (int i = 0; i < table->ucConnectorCount; i++) {
        ATOM_CONNECTOR_OBJECT_RECORD *record = &table->asObjects[i];
        uint16_t objId = record->usConnObjectId;
        uint8_t connType = (objId & OBJECT_TYPE_MASK);
        uint8_t connEnum = (objId & OBJECT_ID_MASK) >> 8;
        
        DBGLOG(AMDSUP, "      [%d] ID: 0x%04X (Type: %s, Enum: %d)", 
               i, objId, getConnectorTypeName(connType), connEnum);
    }
}

// This determines if the driver accepts the card. If this returns NULL, start() is never called.
void *AMDSupport::controllerProbe(void *that, void *provider, int32_t *score) {
    const char *className = "Unknown";
    if (that) {
        OSObject *obj = static_cast<OSObject *>(that);
        if (obj && obj->getMetaClass()) {
            className = obj->getMetaClass()->getClassName();
        }
    }

    // Log the probe attempt
    DBGLOG(AMDSUP, "ATIController::probe() called for class: %s", className);

    void *result = nullptr;
    if (orgControllerProbe) {
        result = FunctionCast(controllerProbe, orgControllerProbe)(that, provider, score);
    }

    int32_t scoreVal = score ? *score : -1;
    DBGLOG(AMDSUP, "ATIController::probe() for %s returned: %p (Score: %d)", className, result, scoreVal);
    
    return result;
}

bool AMDSupport::controllerStart(void *that, void *provider) {
    // Attempt to get the class name to confirm
    const char *className = "Unknown";
    if (that) {
        // Safe cast to IOService/OSObject to get meta class
        OSObject *obj = static_cast<OSObject *>(that);
        if (obj && obj->getMetaClass()) {
            className = obj->getMetaClass()->getClassName();
        }
    }

    DBGLOG(AMDSUP, "ATIController::start() called for class: %s", className);
    bool result = false;
    
    if (orgControllerStart) {
        result = FunctionCast(controllerStart, orgControllerStart)(that, provider);
    }
    
    DBGLOG(AMDSUP, "ATIController::start() for %s returned: %s", className, result ? "Success" : "FAILED");
    
    return result;
}

int AMDSupport::doesSupportsProject(void *that, void *projectName) {
    const char *nameStr = "null";
    if (projectName) {
        OSString *str = OSDynamicCast(OSString, static_cast<OSObject*>(projectName));
        if (str) nameStr = str->getCStringNoCopy();
    }

    int result = 0;
    // Log before call to see what it asks for
    DBGLOG(AMDSUP, "ATIController::doesSupportsProject(Project: '%s') called", nameStr);

    if (orgDoesSupportsProject) {
        result = FunctionCast(doesSupportsProject, orgDoesSupportsProject)(that, projectName);
    }

    DBGLOG(AMDSUP, "ATIController::doesSupportsProject(Project: '%s') -> Result: %d", nameStr, result);
    return result;
}

int AMDSupport::initializeProjectInfo(void *that) {
    int result = 0;
    
    // Call OG first to let it do basic setup
    if (orgInitializeProjectInfo) {
        result = FunctionCast(initializeProjectInfo, orgInitializeProjectInfo)(that);
    }
    
    DBGLOG(AMDSUP, "ATIController::initializeProjectInfo() Original Result: 0x%X", result);

    // If it failed (which it does for RadeonFramebuffer), we intervene
    if (result != 0) {
        DBGLOG(AMDSUP, "ATIController::initializeProjectInfo() failed. Forcing manual 'RadeonFramebuffer' setup...");

        // We need to set specific properties on the IOService (that->provider usually)
        // Accessing 'that' as an IOService
        IOService *service = static_cast<IOService *>(that);
        if (service) {
            service->setProperty("@0,name", OSString::withCString("ATY,RadeonFramebuffer"));
            
            uint32_t refClk = 2700; // 27 MHz * 100
            service->setProperty("ATY,RefCLK", OSData::withBytes(&refClk, sizeof(refClk)));
            service->setProperty("ATY,RefClock", OSData::withBytes(&refClk, sizeof(refClk)));

            service->setProperty("model", OSString::withCString("ATI Radeon HD 5000"));
            service->setProperty("device_type", OSString::withCString("gpu-controller"));
        }

        result = 0;
    }
    
    return result;
}

int AMDSupport::initializeProjectInfoString(void *that, void *projectName) {
    const char *nameStr = "null";
    OSString *str = nullptr;
    
    if (projectName) {
        str = OSDynamicCast(OSString, static_cast<OSObject*>(projectName));
        if (str) nameStr = str->getCStringNoCopy();
    }

    DBGLOG(AMDSUP, "ATIController::initializeProjectInfo(Project: '%s') [String] called", nameStr);

    int result = 0;
    if (orgInitializeProjectInfoString) {
        // Redirect ATY,Hoolock to RadeonFramebuffer
        if (str && (strstr(nameStr, "Hoolock") || strstr(nameStr, "ATY,Hoolock"))) {
            DBGLOG(AMDSUP, "Redirecting '%s' to 'ATY,ATY,RadeonFramebuffer'...", nameStr);
            
            OSString *genericName = OSString::withCString("ATY,ATY,RadeonFramebuffer");
            if (genericName) {
                // Call original just in case it does partial setup, but expect failure
                result = FunctionCast(initializeProjectInfoString, orgInitializeProjectInfoString)(that, genericName);
                genericName->release();
                
                DBGLOG(AMDSUP, "ATIController::initializeProjectInfo('ATY,ATY,RadeonFramebuffer') Original Result: 0x%X", result);
                return 1;
            }
        }
        
        result = FunctionCast(initializeProjectInfoString, orgInitializeProjectInfoString)(that, projectName);
    }

    return result;
}

// Inject generated topology from ATY,RFB (HDMI, DVI, VGA)
int AMDSupport::getConnectorsInfo(void *that, void *connectorInfo, uint8_t &count) {
    
    if (connectorInfo) {
        DBGLOG(AMDSUP, "ATIController::getConnectorsInfo(): Injecting custom connector data...");
        
        auto *connectors = static_cast<LegacyConnector *>(connectorInfo);
        
        // Only clear 3 slots (48 bytes)
        memset(connectors, 0, sizeof(LegacyConnector) * 3);

        // [0] HDMI
        connectors[0].type        = ConnectorHDMI; // 0x800
        connectors[0].flags       = 0x00000204;
        connectors[0].features    = 0x0000;
        connectors[0].priority    = 0;
        connectors[0].sense       = 0x2; 
        connectors[0].hotplug     = 0; 
        connectors[0].transmitter = 33; // 0x21
        connectors[0].encoder     = 3;  // Encoder 3

        // [1] DVI-D
        connectors[1].type        = ConnectorDigitalDVI; // 0x4
        connectors[1].flags       = 0x00000004;
        connectors[1].features    = 0x0000;
        connectors[1].priority    = 0;
        connectors[1].sense       = 0x4;
        connectors[1].hotplug     = 0;
        connectors[1].transmitter = 0;
        connectors[1].encoder     = 0; 

        // [2] VGA
        connectors[2].type        = ConnectorVGA; // 0x10
        connectors[2].flags       = 0x00000010;
        connectors[2].features    = 0x0000;
        connectors[2].priority    = 0;
        connectors[2].sense       = 0x1;
        connectors[2].hotplug     = 0;
        connectors[2].transmitter = 0;
        connectors[2].encoder     = 0;

        count = 3;
        
        dumpRawConnectors(connectorInfo, count);
        
        // Return 0 = kIOReturnSuccess
        return 0;
    }
    
    return -1;
}

void *AMDSupport::getAtomObjectTable(void *that) {
    void *result = nullptr;
    DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTable(): called");

    if (orgGetAtomObjectTable) {
        result = FunctionCast(getAtomObjectTable, orgGetAtomObjectTable)(that);
        DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTable(): got -> %p", result);
    } else {
        DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTable(): failed to run via trampoline!");
        return result;
    }

    return result;
}

void *AMDSupport::getAtomObjectTableForType(void *that, uint8_t type, uint8_t &revision) {
    void *result = nullptr;
    char procName[64] = {0};
    proc_selfname(procName, sizeof(procName));
    int pid = proc_selfpid();
    DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTableForType(): called");

    if (orgGetAtomObjectTableForType) {
        result = FunctionCast(getAtomObjectTableForType, orgGetAtomObjectTableForType)(that, type, revision);
        // Who called it, what type they asked for, what revision was written back, and the resulting pointer
        DBGLOG(AMDSUP, "[%s:%d] getAtomObjectTableForType(Type: 0x%02X) -> Ptr: %p, Rev: 0x%02X", procName, pid, type, result, revision);
    } else {
        DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTableForType(): failed to run via trampoline!");
    }
    
    if (result) {
        dumpConnectorTable(result);
    } else {
        DBGLOG(AMDSUP, "AMDSupport::getAtomObjectTableForType(): No table data to dump!");
        return result;
    }

    return result;
}

int AMDSupport::checkDDCConnections(void *that, void *connector, uint32_t flags, void *edid) {
    int result = 0;
    DBGLOG(AMDSUP, "ATIController::checkDDCConnections called");
    
    // Call Original first to get the result
    if (orgCheckDDCConnections) {
        result = FunctionCast(checkDDCConnections, orgCheckDDCConnections)(that, connector, flags, edid);
    }

    // We log the 'that' pointer to see which controller instance called it, and the result
    DBGLOG(AMDSUP, "ATIController::checkDDCConnections(Conn: %p, Flags: 0x%X, Edid: %p) -> Result: %d", connector, flags, edid, result);
    return result;
}
