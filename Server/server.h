#pragma once
#ifndef SERVER_H
#define SERVER_H

#include <array>
#include <fstream>
#include <string>
#include <memory>
#include <boost/asio.hpp>

namespace server {

	class tcp_session
		: public std::enable_shared_from_this<tcp_session>
	{
	public:
		using TcpSocket = boost::asio::ip::tcp::socket;

		tcp_session(TcpSocket t_socket);

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


	class tcp_server
		: public std::enable_shared_from_this<tcp_session>
	{
	public:
		using TcpSocket = boost::asio::ip::tcp::socket;
		using TcpAcceptor = boost::asio::ip::tcp::acceptor;
		using IoService = boost::asio::io_service;

		tcp_server(IoService& t_ioService, short t_port, std::string const& t_workDirectory);

	private:
		void do_tcp_accept();
		void create_root_directory();

		TcpSocket tcp_socket_;
		TcpAcceptor tcp_acceptor_;

		std::string root_directory_;
		std::string working_directory_;
	};

	using boost::asio::ip::udp;

	class udp_server {
	public:
		using UdpSocket = boost::asio::ip::udp::socket;
		using UdpEndpoint = boost::asio::ip::udp::endpoint;
		using IoService = boost::asio::io_service;

		udp_server(IoService & io_service, short port);

		void do_receive();

		void do_send(std::size_t length);

	private:
		UdpSocket socket_;
		UdpEndpoint sender_endpoint_;
		enum { max_length = 1024 };
		char data_[max_length];
	};
}
#endif // !SERVER_H