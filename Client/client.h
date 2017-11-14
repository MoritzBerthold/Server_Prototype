#pragma once

#include <array>
#include <fstream>

#include <boost/asio.hpp>

void choose_files(std::vector<std::string>& file_paths);

class Client
{
public:
    using IoService = boost::asio::io_service;
	using TcpAcceptor = boost::asio::ip::tcp::acceptor;
    using TcpResolver = boost::asio::ip::tcp::resolver;
    using TcpResolverIterator = TcpResolver::iterator;
    using TcpSocket = boost::asio::ip::tcp::socket;

    Client(IoService& t_ioService, TcpResolverIterator t_endpointIterator,
		std::string const &t_path);

private:
    void open_file(std::string const &t_filepath);
	void do_tcp_cd(std::string const &dir);
	void do_tcp_mkdir(std::string const &dir);
	void do_tcp_accept();
    void do_tcp_connect();
    void do_tcp_write_file(const boost::system::error_code& t_ec);
	void do_tcp_write_request(const boost::system::error_code &ec);
	void tcp_routine();
	template<class Buffer>
    void write_buffer(Buffer& t_buffer);


	TcpAcceptor tcp_acceptor_;
    TcpResolver tcp_resolver_;
    TcpSocket tcp_socket_;
    TcpResolverIterator endpoint_iterator_;
    enum { MessageSize = 16 * 1024 };
    std::array<char, MessageSize> buffer_;
    boost::asio::streambuf request_;
    std::ifstream source_filestream_;
    std::string path_;
};
