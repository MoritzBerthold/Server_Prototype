#include <iostream>

#include <boost/asio/io_service.hpp>

#include "client.h"
#include "../Common/logger.h"


int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: client <address> <port>\n";
        return 1;
    }

    Logger::instance().setOptions("client_%3N.log", 1 * 1024 * 1024, 10 * 1024 * 1024);

    auto address = argv[1];
    auto port = argv[2];

    try {
        boost::asio::io_service ioService;

        boost::asio::ip::tcp::resolver resolver(ioService);
        auto endpointIterator = resolver.resolve({ address, port });

		std::vector<std::string> file_paths;
		choose_files(file_paths);
		std::vector<Client *> clients;
		for (auto s : file_paths) {
			clients.push_back(new Client(ioService, endpointIterator, s));
		}

        ioService.run();
    } catch (std::fstream::failure& e) {
        std::cerr << e.what() << "\n";
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
