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

    if (strcmp(kextList[kextIndex].id, "com.apple.kext.ATISupport") != 0) {
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

// Helper to dbgout the connectors gotten
void AMDSupport::dumpRawConnectors(void *connectorInfo, int count) {
    auto *connectors = static_cast<LegacyConnector *>(connectorInfo);
    DBGLOG(AMDSUP, "Raw Connectors Dump");

    for (int i = 0; i < count; i++) {
        LegacyConnector *c = &connectors[i];
        DBGLOG(AMDSUP, "  [%d] Type:0x%X (%s) Sense:0x%X Flags:0x%X Prio:%d Enc:%d Tx:%d",
               i, c->type, getConnectorTypeName(c->type), c->sense, c->flags, c->priority, c->encoder, c->transmitter);
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
    DBGLOG(AMDSUP, "ATIController::initializeProjectInfo() [Void] called");

    // Call OG function
    if (orgInitializeProjectInfo) {
        result = FunctionCast(initializeProjectInfo, orgInitializeProjectInfo)(that);
    }
    
    // Log result
    DBGLOG(AMDSUP, "ATIController::initializeProjectInfo() [Void] -> Result: 0x%X", result);
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
        // If the driver asks for "ATY,Hoolock", we redirect it to "Radeon"
        // "Radeon" is the internal name for the generic framebuffer that reads the VBIOS.
        // This is being done in an attempt to mitigate issues with connectors
        if (str && (strstr(nameStr, "Hoolock") || strstr(nameStr, "ATY,Hoolock"))) {
            DBGLOG(AMDSUP, "Redirecting '%s' to 'Radeon' (Generic VBIOS Parser)...", nameStr);
            
            OSString *genericName = OSString::withCString("Radeon");
            if (genericName) {
                result = FunctionCast(initializeProjectInfoString, orgInitializeProjectInfoString)(that, genericName);
                genericName->release();
                
                DBGLOG(AMDSUP, "ATIController::initializeProjectInfo('Radeon') [String] -> Result: 0x%X", result);
                return result;
            }
        }
        
        // Default behavior for other projects
        result = FunctionCast(initializeProjectInfoString, orgInitializeProjectInfoString)(that, projectName);
    }

    DBGLOG(AMDSUP, "ATIController::initializeProjectInfo(Project: '%s') [String] -> Result: 0x%X", nameStr, result);
    return result;
}

bool AMDSupport::getConnectorsInfo(void *that, void *connectorInfo, uint8_t &count) {
    // Call Original "Radeon" Generic Parser
    bool result = false;
    if (orgGetConnectorsInfo) {
        result = FunctionCast(getConnectorsInfo, orgGetConnectorsInfo)(that, connectorInfo, count);
    }
    
    DBGLOG(AMDSUP, "ATIController::getConnectorsInfo() Original Result: %s, Count: %d", result ? "True" : "False", count);

    // Patch the Data
    if (connectorInfo && count > 0) {
        auto *connectors = static_cast<LegacyConnector *>(connectorInfo);
        
        DBGLOG(AMDSUP, "ATIController::getConnectorsInfo(): Patching Connectors...");
        
        // Capture valid values from any successful port (e.g., HDMI) to reuse if needed
        uint8_t validEnc = 0;
        uint8_t validTx  = 0;
        for (int i = 0; i < count; i++) {
            if (connectors[i].encoder != 0) {
                validEnc = connectors[i].encoder;
                validTx  = connectors[i].transmitter;
                break;
            }
        }

        // Default fallback if nothing found
        if (validEnc == 0) validEnc = 3;
        if (validTx == 0)  validTx  = 0x21; // 33

        for (int i = 0; i < count; i++) {
            LegacyConnector *conn = &connectors[i];
            
            // Priorities (Priority 0 = Black Screen)
            if (conn->priority == 0) {
                conn->priority = i + 1;
            }

            // Sense IDs (Zero Sense = Driver ignores port)
            if (conn->sense == 0) {
                conn->sense = i + 1;
            }

            // Encoders/Transmitters (Zero = Driver Init Failed)
            if (conn->encoder == 0) {
                if (conn->type == ConnectorVGA) {
                    // VGA usually needs a DAC. Try Encoder 1 (or 0x10 if raw).
                    // Legacy struct uses indices. 1 is often DAC1.
                    conn->encoder = 1; 
                    conn->transmitter = 0; // DACs usually have Tx 0 or 0x10
                    DBGLOG(AMDSUP, "  [%d] VGA: Patched Enc:0->1 Tx:0->0", i);
                } else {
                    // DVI/HDMI/Digital: Reuse the known valid Digital Encoder
                    conn->encoder = validEnc;
                    conn->transmitter = validTx;
                    DBGLOG(AMDSUP, "  [%d] Digital: Patched Enc:0->%d Tx:0->%d", i, validEnc, validTx);
                }
            }

            // Flags to set connectors to HDMI, DVI-D, VGA
            if (conn->flags == 0) {
                if (conn->type == ConnectorHDMI)       conn->flags = 0x00000204;
                else if (conn->type == ConnectorDigitalDVI) conn->flags = 0x00000004;
                else if (conn->type == ConnectorVGA)        conn->flags = 0x00000010;
            }
            
            DBGLOG(AMDSUP, "  [%d] Final: Type:0x%X Sense:0x%X Prio:%d Enc:%d Tx:%d", 
                   i, conn->type, conn->sense, conn->priority, conn->encoder, conn->transmitter);
        }
        
        // Force Success so driver accepts our patched table
        result = true;
    } else {
        DBGLOG(AMDSUP, "ATIController::getConnectorsInfo(): returned no data to patch.");
    }

    return result;
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
