#pragma once

#include <lib/handle.hpp>
#include <lib/types.hpp>

void dmesg_enable();
void dmesg_disable();

class DMesg : public Handle {

public:
    ssize_t write(const void *, size_t) override;
};
