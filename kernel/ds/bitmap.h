#pragma once

#include <stdint.h>

class bitmap_t {
private:
    uint8_t *m_bitmap;
    uint64_t m_size;

public:
    bitmap_t() = default;
    bitmap_t(uint8_t *bitmap, uint64_t size);

    bool test(uint64_t index, bool value);
    bool test_range(uint64_t index, uint64_t size, bool value);

    void set(uint64_t index, bool value);
    void set_range(uint64_t index, uint64_t size, bool value);

    uint64_t find_first(uint64_t index, bool value);
    uint64_t find_first_range(uint64_t index, uint64_t size, bool value);
};
