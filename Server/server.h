#pragma once

#include <array>
#include <fstream>
#include <string>
#include <memory>
#include <boost/asio.hpp>


class Session
    : public std::enable_shared_from_this<Session>
{
public:
    using TcpSocket = boost::asio::ip::tcp::socket;

    Session(TcpSocket t_socket);

    void start()
    {
        do_tcp_read();
    }

private:
    void do_tcp_read();
    void do_tcp_process_read(size_t t_bytesTransferred);
    void create_file();
    void do_tcp_read_data(std::istream &stream);
    void do_tcp_read_file_content(size_t t_bytesTransferred);
    void handle_error(std::string const& t_functionName, boost::system::error_code const& t_ec);
	void cd(std::string dir);
	void mkdir(std::string name);

    TcpSocket tcp_socket_;
    enum { MaxLength = 40960 };
    std::array<char, MaxLength> buffer_;
    boost::asio::streambuf request_buffer_;
    std::ofstream output_filestream_;
    size_t file_size_;
    std::string file_name_;
	std::string request_type_;
};


class Server
	: public std::enable_shared_from_this<Session>
{
public:
    using TcpSocket = boost::asio::ip::tcp::socket;
    using TcpAcceptor = boost::asio::ip::tcp::acceptor;
    using IoService = boost::asio::io_service;

    Server(IoService& t_ioService, short t_port, std::string const& t_workDirectory);

private:
    void do_tcp_accept();
    void create_root_directory();

    TcpSocket tcp_socket_;
    TcpAcceptor tcp_acceptor_;

    std::string root_directory_;
	std::string working_directory_;
};
