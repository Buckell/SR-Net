#include <iostream>
#include <net.hpp>

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
        net.write_int(13);
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