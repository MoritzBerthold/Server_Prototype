#pragma once

#include <array>
#include <fstream>

#include <boost/asio.hpp>

void choose_files(std::vector<std::string>& file_paths);

class Client
{
public:
    using IoService = boost::asio::io_service;
    using TcpResolver = boost::asio::ip::tcp::resolver;
    using TcpResolverIterator = TcpResolver::iterator;
    using TcpSocket = boost::asio::ip::tcp::socket;

    Client(IoService& t_ioService, TcpResolverIterator t_endpointIterator,
		std::string const &t_path);

private:
    void openFile(std::string const &t_filepath);
    void doConnect();
    void doWriteFile(const boost::system::error_code& t_ec);
	void routine();
	template<class Buffer>
    void writeBuffer(Buffer& t_buffer);


    TcpResolver m_ioService;
    TcpSocket m_socket;
    TcpResolverIterator m_endpointIterator;
    enum { MessageSize = 1 * 1024 };
    std::array<char, MessageSize> m_buf;
    boost::asio::streambuf m_request;
    std::ifstream m_sourceFile;
    std::string m_path;
};
