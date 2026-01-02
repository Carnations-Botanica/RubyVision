//
//  AMDSupport.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/31/25.
//

#ifndef AMDSupport_h
#define AMDSupport_h

// Base Includes
#include <Headers/kern_patcher.hpp>
#include <libkern/c++/OSObject.h>
#include <Headers/kern_util.hpp>
#include <Headers/kern_api.hpp>
#include <sys/proc.h>
#include "kern_start.hpp"

// Logging Defs
#define AMDSUP "AMDSU"

class AMDSupport {
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
    static void *getAtomObjectTable(void *that);
    static void *getAtomObjectTableForType(void *that, uint8_t type, uint8_t &revision);
    static int checkDDCConnections(void *that, void *connector, uint32_t flags, void *edid);
    static int getConnectorsInfo(void *that, void *connectorInfo, uint8_t &count);
    static int doesSupportsProject(void *that, void *projectName);
    static int initializeProjectInfo(void *that);
    static int initializeProjectInfoString(void *that, void *projectName);

};

#endif /* AMDSupport_h */
