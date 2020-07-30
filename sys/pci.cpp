#include <sys/pci.hpp>
#include <lib/print.hpp>
#include <lib/dynarray.hpp>
#include <sys/port_io.hpp>

static constexpr size_t PCI_MAX_FUNCTION = 8;
static constexpr size_t PCI_MAX_DEVICE   = 32;
static constexpr size_t PCI_MAX_BUS      = 256;
static DynArray<PCIDevice> device_list;

PCIDevice::PCIDevice(uint8_t b, uint8_t d, uint8_t f, uint8_t c, uint8_t sc,
                     uint8_t pi) {
    bus      = b;
    device   = d;
    function = f;
    _class   = c;
    subclass = sc;
    prog_if  = pi;
}

uint8_t PCIDevice::get_bus() const {
    return bus;
}

uint8_t PCIDevice::get_device() const {
    return device;
}

uint8_t PCIDevice::get_function() const {
    return function;
}

uint8_t PCIDevice::get_class() const {
    return _class;
}

uint8_t PCIDevice::get_subclass() const {
    return subclass;
}

uint8_t PCIDevice::get_prog_if() const {
    return prog_if;
}

void PCIDevice::prepare_config(uint32_t offset) {
    uint32_t address = (bus << 16) | (device << 11) | (function << 8)
                     | (offset & ~((uint32_t)(3))) | 0x80000000;
    outd(0xcf8, address);
}

uint8_t PCIDevice::readb(uint32_t offset) {
    prepare_config(offset);
    return inb(0xcfc + (offset & 3));
}

void PCIDevice::writeb(uint32_t offset, uint8_t value) {
    prepare_config(offset);
    outb(0xcfc + (offset & 3), value);
}

uint16_t PCIDevice::readw(uint32_t offset) {
    prepare_config(offset);
    return inw(0xcfc + (offset & 3));
}

void PCIDevice::writew(uint32_t offset, uint16_t value) {
    prepare_config(offset);
    outw(0xcfc + (offset & 3), value);
}

uint32_t PCIDevice::readd(uint32_t offset) {
    prepare_config(offset);
    return ind(0xcfc + (offset & 3));
}

void PCIDevice::writed(uint32_t offset, uint32_t value) {
    prepare_config(offset);
    outd(0xcfc + (offset & 3), value);
}

int PCIDevice::read_bar(size_t bar, PCIDeviceBar &out) {
    if (bar > 5) {
        return -1;
    }

    size_t reg_index       = 0x10 + bar * 4;
    uint64_t bar_low       = readd(reg_index);
    uint64_t bar_size_low  = 0;
    uint64_t bar_high      = 0;
    uint64_t bar_size_high = 0;

    if (!bar_low) {
        return -1;
    }

    uintptr_t base;
    size_t    size;

    bool is_mmio         = !(bar_low & 1);
    bool is_prefetchable = is_mmio && bar_low & (1 << 3);
    bool is_64bit        = is_mmio && ((bar_low >> 1) & 0b11) == 0b10;

    if (is_64bit) {
        bar_high = readd(reg_index + 4);
    }

    base = ((bar_high << 32) | bar_low) & ~(is_mmio ? (0b1111) : (0b11));

    writed(reg_index, 0xFFFFFFFF);
    bar_size_low = readd(reg_index);
    writed(reg_index, bar_low);

    if (is_64bit) {
        writed(reg_index + 4, 0xFFFFFFFF);
        bar_size_high = readd(reg_index + 4);
        writed(reg_index + 4, bar_high);
    }

    size = ((bar_size_high << 32) | bar_size_low) & ~(is_mmio ? (0b1111) : (0b11));
    size = ~size + 1;
    out  = (PCIDeviceBar){base, size, is_mmio, is_prefetchable};

    return 0;
}

int PCIDevice::register_msi(uint8_t vector, uint8_t lapic_id) {
    uint8_t off = 0;

    uint32_t config_4  = readd(PCI_HAS_CAPS);
    uint8_t  config_34 = readb(PCI_CAPS);

    if ((config_4 >> 16) & (1 << 4)) {
        uint8_t cap_off = config_34;

        while (cap_off) {
            uint8_t cap_id   = readb(cap_off);
            uint8_t cap_next = readb(cap_off + 1);

            switch (cap_id) {
                case 0x05: {
                    print("pci: device has msi support\n");
                    off = cap_off;
                    break;
                }
            }
            cap_off = cap_next;
        }
    }

    if (off == 0) {
        print("pci: device does not support msi\n");
        return 0;
    }

    uint16_t msi_opts = readw(off + MSI_OPT);
    if (msi_opts & MSI_64BIT_SUPPORTED) {
        msi_data_t data    = {.raw = 0};
        msi_address_t addr = {.raw = 0};
        addr.raw = readw(off + MSI_ADDR_LOW);
        data.raw = readw(off + MSI_DATA_64);
        data.vector = vector;
        //Fixed delivery mode
        data.delivery_mode = 0;
        addr.base_address = 0xFEE;
        addr.destination_id = lapic_id;
        writed(off + MSI_ADDR_LOW, addr.raw);
        writed(off + MSI_DATA_64, data.raw);
    } else {
        msi_data_t data    = {.raw = 0};
        msi_address_t addr = {.raw = 0};
        addr.raw = readw(off + MSI_ADDR_LOW);
        data.raw = readw(off + MSI_DATA_32);
        data.vector = vector;
        //Fixed delivery mode
        data.delivery_mode = 0;
        addr.base_address = 0xFEE;
        addr.destination_id = lapic_id;
        writed(off + MSI_ADDR_LOW, addr.raw);
        writed(off + MSI_DATA_32, data.raw);
    }
    msi_opts |= 1;
    writew(off + MSI_OPT, msi_opts);
    return 1;
}

static void pci_check_bus(uint8_t bus);

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func) {
    auto base_device  = PCIDevice(bus, slot, func, 0, 0, 0);
    uint32_t config_0 = base_device.readd(0);

    if (config_0 == 0xffffffff) {
        return;
    }

    uint32_t config_8 = base_device.readd(0x8);
    auto _class       = (uint8_t)(config_8 >> 24);
    auto subclass     = (uint8_t)(config_8 >> 16);
    auto prog_if      = (uint8_t)(config_8 >> 8);

    // Check it's not a PCI bridge.
    auto final_device = PCIDevice(bus, slot, func, _class, subclass, prog_if);
    if (_class == 0x06 && subclass == 0x04) {
        uint32_t config_18 = final_device.readd(0x18);
        pci_check_bus((config_18 >> 8) & 0xFF);
    } else {
        device_list.push_back(final_device);
    }
}

static void pci_check_bus(uint8_t bus) {
    for (size_t dev = 0; dev < PCI_MAX_DEVICE; dev++) {
        for (size_t func = 0; func < PCI_MAX_FUNCTION; func++) {
            pci_check_function(bus, dev, func);
        }
    }
}

static void pci_init_root_bus(void) {
    auto base_device = PCIDevice(0, 0, 0, 0, 0, 0);
    uint32_t config_c = base_device.readd(0xc);
    uint32_t config_0;

    if (!(config_c & 0x800000)) {
        pci_check_bus(0);
    } else {
        for (size_t func = 0; func < 8; func++) {
            base_device = PCIDevice(0, 0, func, 0, 0, 0);
            config_0 = base_device.readd(0);

            if (config_0 == 0xffffffff)
                continue;

            pci_check_bus(func);
        }
    }
}

void pci_init() {
    pci_init_root_bus();

    auto len = device_list.length();
    print("pci: Detected %U PCI devices:\n", len);
    for (size_t i = 0; i < len; i++) {
        print("pci: Device %U:\n", i);
        print("pci: ..Bus:      %U\n", device_list[i].get_bus());
        print("pci: ..Device:   %U\n", device_list[i].get_device());
        print("pci: ..Function: %X\n", device_list[i].get_function());
        print("pci: ..Class:    %X\n", device_list[i].get_class());
        print("pci: ..Subclass: %X\n", device_list[i].get_subclass());
        print("pci: ..Prog IF:  %X\n", device_list[i].get_prog_if());
    }
}

PCIDevice *pci_get_device(uint8_t cl, uint8_t subcl, uint8_t prog_if) {
    auto len = device_list.length();
    for (size_t i = 0; i < len; i++) {
        auto current_class    = device_list[i].get_class();
        auto current_subclass = device_list[i].get_subclass();
        auto current_prog_if  = device_list[i].get_prog_if();

        if (current_class == cl && current_subclass == subcl &&
            current_prog_if == prog_if) {
            return &device_list[i];
        }
    }

    return nullptr;
}
