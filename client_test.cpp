#include <net.hpp>
#include "test_serial.hpp"

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
        std::cout << net.read_int() << std::endl;
        std::cout << net.is_next_string() << std::endl;
        std::cout << net.peek_string() << std::endl;
        std::cout << net.read_string() << std::endl;
        std::cout << net.read_bool() << std::endl;
        std::cout << net.read_bool() << std::endl;
        net.null_read_int();
        net.null_read_string();
        std::cout << net.read_int() << std::endl;
        std::cout << net.read_float() << std::endl;
        auto bytes = net.read_bytes();
        std::for_each(bytes.begin(), bytes.end(), [](auto b) {
            std::cout << std::hex << (size_t)b << ' ';
        });

        std::cout << std::endl << std::dec;

        test_serial dd(1, 2, 3);
        net.read(dd);
        dd.print();
    });

    net.start_sync();
}