#include <iostream>

#include <boost/asio/read_until.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "server.h"

void print_dir(std::string const& path, std::string const& präfix = "", int depth = 0){
	std::cout << präfix << char(200) << path.substr(path.find_last_of('\\')+1)<< '\\' << std::endl;
	
	boost::filesystem::directory_iterator end_itr;
	for (boost::filesystem::directory_iterator itr(path); itr != end_itr && depth < 1;)
	{
		auto curr_path = itr->path();

		std::string curr_prefix;
		if (++itr == end_itr) {
			curr_prefix = "   " + präfix + char(200);
		}
		else {
			curr_prefix = "   " + präfix + char(204);
		}

		if (is_regular_file(curr_path)) {
			std::string current_file = curr_path.string();
			std::cout << curr_prefix << current_file.substr(current_file.find_last_of('\\') + 1) << std::endl;
		}
		if (is_directory(curr_path)) {
			print_dir(curr_path.string(), "   " + präfix, depth + 1);
		}
	}
}

namespace server {

	tcp_session::tcp_session(TcpSocket t_socket)
		: tcp_socket_(std::move(t_socket))
	{
	}


	void tcp_session::do_tcp_read()
	{
		auto self = shared_from_this();
		async_read_until(tcp_socket_, request_buffer_, "\n\n",
			[this, self](boost::system::error_code ec, size_t bytes)
		{
			if (!ec)
				do_tcp_process_read(bytes);
			else
				handle_error(__FUNCTION__, ec);
		});
	}


	void tcp_session::do_tcp_process_read(size_t t_bytesTransferred)
	{
		BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << "(" << t_bytesTransferred << ")"
			<< ", in_avail = " << request_buffer_.in_avail() << ", size = "
			<< request_buffer_.size() << ", max_size = " << request_buffer_.max_size() << ".";

		std::istream requestStream(&request_buffer_);
		do_tcp_read_data(requestStream);

		auto pos = file_name_.find_last_of('\\');
		if (pos != std::string::npos)
			file_name_ = file_name_.substr(pos + 1);

		create_file();

		// write extra bytes to file
		do {
			requestStream.read(buffer_.data(), buffer_.size());
			//BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << " write " << requestStream.gcount() << " bytes.";
			output_filestream_.write(buffer_.data(), requestStream.gcount());
		} while (requestStream.gcount() > 0);

		auto self = shared_from_this();
		tcp_socket_.async_read_some(boost::asio::buffer(buffer_.data(), buffer_.size()),
			[this, self](boost::system::error_code ec, size_t bytes)
		{
			if (!ec)
				do_tcp_read_file_content(bytes);
			else
				handle_error(__FUNCTION__, ec);
		});
	}


	void tcp_session::do_tcp_read_data(std::istream &stream)
	{
		stream >> request_type_;
		BOOST_LOG_TRIVIAL(trace) << "Received request of type: " << request_type_;
		if (request_type_ == "FILE_WRITE") {
			stream >> file_name_;
			stream >> file_size_;
			stream.read(buffer_.data(), 2);

			BOOST_LOG_TRIVIAL(trace) << file_name_ << " size is " << file_size_
				<< ", tellg = " << stream.tellg();
		}
		if (request_type_ == "CHANGE_DIRECTORY") {
			std::string dir;
			stream >> dir;
			cd(dir);
		}
		if (request_type_ == "MAKE_DIRECTORY") {
			std::string dir;
			stream >> dir;
			mkdir(dir);
		}
	}


	void tcp_session::create_file()
	{
		output_filestream_.open(file_name_, std::ios_base::binary);
		if (!output_filestream_) {
			BOOST_LOG_TRIVIAL(error) << __LINE__ << ": Failed to create: " << file_name_;
			return;
		}
	}


	void tcp_session::do_tcp_read_file_content(size_t t_bytesTransferred)
	{
		if (t_bytesTransferred > 0) {
			output_filestream_.write(buffer_.data(), static_cast<std::streamsize>(t_bytesTransferred));

			BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << " recv " << output_filestream_.tellp() << " bytes";

			if (output_filestream_.tellp() >= static_cast<std::streamsize>(file_size_)) {
				std::cout << "Received file: " << file_name_ << std::endl;
				output_filestream_.close();
				return;
			}
		}
		auto self = shared_from_this();
		tcp_socket_.async_read_some(boost::asio::buffer(buffer_.data(), buffer_.size()),
			[this, self](boost::system::error_code ec, size_t bytes)
		{
			do_tcp_read_file_content(bytes);
		});
	}


	void tcp_session::handle_error(std::string const& t_functionName, boost::system::error_code const& t_ec)
	{
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << " in " << t_functionName << " due to "
			<< t_ec << " " << t_ec.message() << std::endl;
	}


	tcp_server::tcp_server(IoService& t_ioService, short t_port, std::string const& t_workDirectory)
		: tcp_socket_(t_ioService),
		tcp_acceptor_(t_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), t_port)),
		root_directory_(t_workDirectory), working_directory_(t_workDirectory)
	{
		std::cout << "Server started" << std::endl << std::endl;

		create_root_directory();

		std::cout << "Contents of root directory:" << std::endl;
		print_dir(working_directory_);

		do_tcp_accept();
	}


	void tcp_server::do_tcp_accept()
	{
		tcp_acceptor_.async_accept(tcp_socket_,
			[this](boost::system::error_code ec)
		{
			if (!ec)
				std::make_shared<tcp_session>(std::move(tcp_socket_))->start();

			do_tcp_accept();
		});
	}

	void tcp_server::create_root_directory()
	{
		using namespace boost::filesystem;
		auto currentPath = path(root_directory_);
		if (!exists(currentPath) && !create_directory(currentPath))
			BOOST_LOG_TRIVIAL(error) << "Couldn't create working directory: " << root_directory_;
		current_path(currentPath);
		working_directory_ = currentPath.string();
	}

	void tcp_session::cd(std::string dir)
	{
		using namespace boost::filesystem;

		std::string dir_name;
		if (dir.find_last_of('\\') == std::string::npos)
			dir_name = dir;
		else
			dir_name = dir.substr(dir.find_last_of('\\'));

		auto dir_path = path(current_path().string() + '\\' + dir_name);
		if (!exists(dir_path)) {
			BOOST_LOG_TRIVIAL(error) << "Unable to change to directory: " << dir_name;
			std::cout << "Unable to change to directory: " << dir_name << std::endl;
			return;
		}
		else {
			std::cout << "Changed working directory to: " << dir_path.string() << std::endl;
			current_path(dir_path);
		}
		std::cout << "Contents of current directory:" << std::endl;
		print_dir(current_path().string());

	}

	void tcp_session::mkdir(std::string name)
	{
		using namespace boost::filesystem;

		std::string dir_name;
		if (name.find_last_of('\\') == std::string::npos)
			dir_name = name;
		else
			dir_name = name.substr(name.find_last_of('\\'));

		auto dir_path = path(current_path().string() + '\\' + dir_name);
		if (!exists(dir_path) && !create_directory(dir_path))
			BOOST_LOG_TRIVIAL(error) << "Couldn't create directory: " << dir_name;
		else
			std::cout << "Created subdirectory: " << dir_name << std::endl;
		std::cout << "Contents of current directory:" << std::endl;
		print_dir(current_path().string());
		return;
	}

	udp_server::udp_server(IoService & io_service, short port)
		: socket_(io_service, udp::endpoint(udp::v4(), port))
	{
	}

	void udp_server::do_receive()
	{
		socket_.async_receive_from(
			boost::asio::buffer(data_, max_length), sender_endpoint_,
			[this](boost::system::error_code ec, std::size_t bytes_recvd)
		{
			if (!ec && bytes_recvd > 0)
			{
				do_send(bytes_recvd);
			}
			else
			{
				do_receive();
			}
		});
	}

	void udp_server::do_send(std::size_t length)
	{
		socket_.async_send_to(
			boost::asio::buffer(data_, length), sender_endpoint_,
			[this](boost::system::error_code /*ec*/, std::size_t /*bytes_sent*/)
		{
			do_receive();
		});
	}
}