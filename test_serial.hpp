//
// Created by maxng on 1/1/2023.
//

#ifndef SR_SERVER_TEST_TEST_SERIAL_HPP
#define SR_SERVER_TEST_TEST_SERIAL_HPP

#include <net.hpp>

struct test_serial : public sr::serializable {
    size_t a = 10;
    size_t b = 20;
    size_t c = 30;

    test_serial(size_t aa, size_t bb, size_t cc) : a(aa), b(bb), c(cc) {}

    void print() const {
        std::cout << a << ' ' << b << ' ' << c << std::endl;
    }

    [[nodiscard]] size_t serialization_id() const noexcept override {
        return 0x103859200193;
    }

    [[nodiscard]] size_t serialization_size() const noexcept override {
        return sizeof(test_serial);
    }

    void serialize(void* a_data) const noexcept override {
        std::memcpy(a_data, reinterpret_cast<const void*>(this), sizeof(test_serial));
    }

    void deserialize(void* a_data, size_t a_size) noexcept override {
        std::memcpy(reinterpret_cast<void*>(this), a_data, sizeof(test_serial));
    }
};

#endif //SR_SERVER_TEST_(void*)TEST_SERIAL_HPP
