#include <iostream>
#include <net.hpp>
#include "test_serial.hpp"

int main() {
    sr::server::net net;
    net.open(2015);

    net.add_network_string("TestServerMessage");
    net.add_network_string("TestClientMessage");

    net.on_connect([&net](auto& client) {
        std::cout << "Connected" << std::endl;
    });

    net.on_ready([&net](auto& client) {
        net.start("TestClientMessage");
        net.write_int(10);
        net.write_string(" hello ");
        net.write_bool(true);
        net.write_bool(false);
        net.write_int(17);
        net.write_string("Hello, world!");
        net.write_int(16);
        net.write_float(15.00234);
        net.write_bytes({
            0x01, 0x03, 0x02, 0x05, 0x07, 0x10, 0x30
        });

        test_serial d(11, 22, 33);
        net.write(d);
        d.print();

        net.send(client);
    });

    net.receive("TestServerMessage", [&net](auto& cl){
        auto a1 = net.read_string();
        auto a2 = net.read_int();
        auto a3 = net.read_string();

        std::cout << a1 << a2 << a3 << std::endl;
    });

    net.start_sync();
}