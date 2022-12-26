#include <net.hpp>

int main() {
    sr::client::net net;
    net.connect("localhost", 2015);

    net.on_ready([&net](){
        std::cout << "READY!" << std::endl;

        net.start("TestServerMessage");
        net.write_string("hello ");
        net.write_int(10);
        net.write_string(" world");
        net.send();
    });

    net.receive("TestClientMessage", [&net](){
        auto a1 = net.read_int();
        auto a2 = net.read_string();
        auto a3 = net.read_int();

        std::cout << a1 << a2 << a3 << std::endl;
    });

    net.start_sync();
}