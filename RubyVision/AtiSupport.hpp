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
#define ATISUP "ATISU"

// Thanks to WhateverGreen and its contributors for the previous research which helped
// provide people with this data ! A lot of ATI/AMD code from WEG is very helpful on this
#pragma pack(push, 1)
struct ATOM_COMMON_TABLE_HEADER {
    uint16_t usStructureSize;
    uint8_t  ucTableFormatRevision;
    uint8_t  ucTableContentRevision;
};

struct ATOM_CONNECTOR_OBJECT_RECORD {
    uint16_t usConnObjectId; // ID (Type + Enum)
    uint16_t usConnectorObjectRecordOffset; // Offset to detailed info
};

struct ATOM_CONNECTOR_OBJECT_TABLE {
    ATOM_COMMON_TABLE_HEADER sHeader;
    uint8_t ucConnectorCount;
    ATOM_CONNECTOR_OBJECT_RECORD asObjects[];
};
#pragma pack(pop)

// Connector Object ID Macros to decode usConnObjectId
#define OBJECT_TYPE_MASK             0x00FF
#define OBJECT_ID_MASK               0xFF00
#define OBJECT_TYPE_SHIFT            0

// Common Object Types
#define GRAPH_OBJECT_TYPE_CONNECTOR  0x71
#define GRAPH_OBJECT_TYPE_ENCODER    0x73

// Connector Types
enum ConnectorType {
    ConnectorLVDS       = 0x2,
    ConnectorDigitalDVI = 0x4,
    ConnectorSVID       = 0x8,
    ConnectorVGA        = 0x10,
    ConnectorDP         = 0x400,
    ConnectorHDMI       = 0x800,
    ConnectorAnalogDVI  = 0x2000
};

// Connector Structure (16 Bytes)
struct LegacyConnector {
    uint32_t type;        // 0x00
    uint32_t flags;       // 0x04
    uint16_t features;    // 0x08
    uint16_t priority;    // 0x0A
    uint8_t transmitter;  // 0x0C
    uint8_t encoder;      // 0x0D
    uint8_t hotplug;      // 0x0E
    uint8_t sense;        // 0x0F
};

class AtiSupport {
public:
    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    // Trampolines
    static mach_vm_address_t orgControllerStart;
    static mach_vm_address_t orgControllerProbe;
    static mach_vm_address_t orgGetConnectorsInfo;
    static mach_vm_address_t orgGetAtomObjectTable;
    static mach_vm_address_t orgGetAtomObjectTableForType;
    static mach_vm_address_t orgCheckDDCConnections;
    static mach_vm_address_t orgDoesSupportsProject;
    static mach_vm_address_t orgInitializeProjectInfo;
    static mach_vm_address_t orgInitializeProjectInfoString;

    // Helpers
    static void dumpConnectorTable(void *tableData);
    static void dumpRawConnectors(void *connectorInfo, int count);
    static const char* getConnectorTypeName(uint32_t type);

    // Reimplementations
    static bool controllerStart(void *that, void *provider);
    static void *controllerProbe(void *that, void *provider, int32_t *score);
    static bool getConnectorsInfo(void *that, void *connectorInfo, uint8_t &count);
    static void *getAtomObjectTable(void *that);
    static void *getAtomObjectTableForType(void *that, uint8_t type, uint8_t &revision);
    static int checkDDCConnections(void *that, void *connector, uint32_t flags, void *edid);
    static int doesSupportsProject(void *that, void *projectName);
    static int initializeProjectInfo(void *that);
    static int initializeProjectInfoString(void *that, void *projectName);

};

#endif /* AtiSupport_h */