#include <iostream>

#include <boost/asio/io_service.hpp>

#include "server.h"
#include "../Common/logger.h"

using namespace server;

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: server <port> <workDirectory>\n";
        return 1;
    }

    Logger::instance().setOptions("server_%3N.log", 1 * 1024 * 1024, 10 * 1024 * 1024);

    try {
        boost::asio::io_service ioService;

        tcp_server server(ioService, std::stoi(argv[1]), argv[2]);
		udp_server udp_server_(ioService, std::stoi(argv[1]));

        ioService.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
