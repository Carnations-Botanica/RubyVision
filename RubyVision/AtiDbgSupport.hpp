//
//  AtiDbgSupport.hpp
//  RubyVision
//
//  Created by RoyalGraphX on 12/29/25.
//

#ifndef AtiDbgSupport_h
#define AtiDbgSupport_h

#include <Headers/kern_patcher.hpp>

class AtiDbgSupport {
public:
    static void init();
    static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
    static mach_vm_address_t orgGetConnectorsInfo;
    static mach_vm_address_t orgCreateAtomBiosProxy;

    struct ConnectorInfo {
        uint32_t type;
        uint32_t flags;
        uint16_t features;
        uint16_t reserved;
        uint8_t transmitter;
        uint8_t encoder;
        uint8_t senseId;
        uint8_t hotplugId;
    };

    static int getConnectorsInfo(void *that, ConnectorInfo *infoBuffer, uint8_t *countRef);
    static void *createAtomBiosProxy(uint32_t length, uint8_t *buffer, void *service, void *allocator);
    
};

#endif /* AtiDbgSupport_h */