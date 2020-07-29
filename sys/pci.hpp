#pragma once

#include <cstddef>
#include <stdint.h>

struct PCIDeviceBar {
    size_t base;
    size_t size;
    bool   is_mmio;
    bool   is_prefetchable;
};

class PCIDevice {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t _class;
    uint8_t subclass;
    uint8_t prog_if;

    void prepare_config(uint32_t offset);

public:
    PCIDevice(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

    uint8_t get_bus()      const;
    uint8_t get_device()   const;
    uint8_t get_function() const;
    uint8_t get_class()    const;
    uint8_t get_subclass() const;
    uint8_t get_prog_if()  const;

    uint8_t  readb(uint32_t offset);
    void     writeb(uint32_t offset, uint8_t value);
    uint16_t readw(uint32_t offset);
    void     writew(uint32_t offset, uint16_t value);
    uint32_t readd(uint32_t offset);
    void     writed(uint32_t offset, uint32_t value);
    int      read_bar(size_t index, PCIDeviceBar &bar);
};

void pci_init();
PCIDevice *pci_get_device(uint8_t cl, uint8_t subcl, uint8_t prog_if);
