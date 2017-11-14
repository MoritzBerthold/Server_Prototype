#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stack>

#include <boost\filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "client.h"
#include "nfd.h"

std::string replace_all(std::string const& s, const char o, const char n) {
	std::stringstream ss;
	for (auto &c : s) {
		if (c == o)
			ss << n;
		else
			ss << c;
	}
	return ss.str();
}

void choose_files(std::stack<std::string>& file_paths)
{
	nfdpathset_t outPaths;
	nfdresult_t result = NFD_OpenDialogMultiple(nullptr, nullptr, &outPaths);

	if (result == NFD_OKAY) {
		size_t curr_offset = 0;
		for (size_t i = 0; i < NFD_PathSet_GetCount(&outPaths); ++i) {
			file_paths.push(NFD_PathSet_GetPath(&outPaths, i));
		}
	}
	else if (result == NFD_CANCEL) {
		puts("Die Dateiauswahl wurde durch den Nutzer abgebrochen.");
		exit(-1);
	}
	else {
		printf("Error: %s\n", NFD_GetError());
	}
}

Client::Client(IoService& t_ioService, TcpResolverIterator t_endpointIterator, 
	std::string const &t_path)
    : tcp_resolver_(t_ioService), tcp_socket_(t_ioService), tcp_acceptor_(t_ioService),
    endpoint_iterator_(t_endpointIterator), path_(t_path)
{
	tcp_routine();
}

void Client::tcp_routine()
{
	work_in_progress = !paths.empty();

	while (!work_in_progress) {
		std::cout << "Client>";
		std::cin >> current_command;
		if (current_command == "upload") {
			choose_files(paths);
			current_write_func = &Client::handler_tcp_send_file;
			work_in_progress = true;
		}
		else if (current_command == "mkdir") {
			current_write_func = &Client::handler_tcp_send_request;
			work_in_progress = true;
		}
		else if (current_command == "cd") {
			current_write_func = &Client::handler_tcp_send_request;
			work_in_progress = true;
		}
		else if (current_command == "quit" || current_command == "exit") {
			break;
		}
		else if (current_command == "help" || current_command == "h" || current_command == "?") {
			std::cout << "Available commands:\n"
				<< "\tupload:\t\tChoose files for upload to server\n"
				<< "\tmkdir:\t\tCreate directory in servers current working directory\n"
				<< "\thelp, h, ?:\t\tPrints this page\n";
		}
		else {
			std::cout << "Available commands:\n"
				<< "\tupload:\t\tChoose files for upload to server\n"
				<< "\tmkdir:\t\tCreate directory in servers current working directory\n"
				<< "\thelp, h, ?:\t\tPrints this page\n";
		}
	}
	if (current_command == "upload") {
		do_tcp_connect();
		open_file(paths.top());
		paths.pop();
	}
	if (current_command == "mkdir") {
		do_tcp_connect();
		std::string dir;
		std::cin >> dir;
		do_tcp_mkdir(dir);
	}
	if (current_command == "cd") {
		do_tcp_connect();
		std::string dir;
		std::cin >> dir;
		do_tcp_cd(dir);
	}
}

void Client::open_file(std::string const &t_filepath)
{
	path_ = t_filepath;

	auto file_dir = path_.substr(0, path_.find_last_of('\\') + 1);
	auto file_name = path_.substr(path_.find_last_of('\\') + 1);
	auto new_file_name = replace_all(file_name, ' ', '_');
	auto new_file_path = file_dir + new_file_name;

	if (path_ != new_file_path) {
		boost::filesystem::rename(boost::filesystem::path(path_), boost::filesystem::path(new_file_path));
		std::cout << "Renamed file to: " << new_file_name << std::endl;
		path_ = new_file_path;
	}

    source_filestream_.open(path_, std::ios_base::binary | std::ios_base::ate);
    if (source_filestream_.fail())
        throw std::fstream::failure("Failed while opening file " + path_);

    source_filestream_.seekg(0, source_filestream_.end);
    auto fileSize = source_filestream_.tellg();
    source_filestream_.seekg(0, source_filestream_.beg);

    std::ostream requestStream(&request_);
    boost::filesystem::path p(path_);
    requestStream << "FILE_WRITE\n" << p.filename().string() << "\n" << fileSize << "\n\n";
    //BOOST_LOG_TRIVIAL(trace) << "Request size: " << m_request.size();
}

void Client::do_tcp_cd(std::string const & dir)
{
	std::ostream requestStream(&request_);
	boost::filesystem::path p(path_);
	requestStream << "CHANGE_DIRECTORY\n" << dir << "\n\n";
}

void Client::do_tcp_mkdir(std::string const & dir)
{
	std::ostream requestStream(&request_);
	boost::filesystem::path p(path_);
	requestStream << "MAKE_DIRECTORY\n" << dir << "\n\n";
}

void Client::do_tcp_accept()
{
	tcp_acceptor_.async_accept(tcp_socket_,
		[this](boost::system::error_code ec)
	{
		if (!ec)
			//std::make_shared<Session>(std::move(tcp_socket_))->start();

		do_tcp_accept();
	});
}

void Client::do_tcp_connect()
{
    boost::asio::async_connect(tcp_socket_, endpoint_iterator_, 
        [this](boost::system::error_code ec, TcpResolverIterator)
        {
            if (!ec) {
                write_buffer(request_);
            } else {
                std::cout << "Couldn't connect to host. Please run server "
                    "or check network connection.\n";
                BOOST_LOG_TRIVIAL(error) << "Error: " << ec.message();
            }
        });
}


void Client::handler_tcp_send_file(const boost::system::error_code& t_ec)
{
    if (!t_ec) {
        if (source_filestream_) {
            source_filestream_.read(buffer_.data(), buffer_.size()); 
            if (source_filestream_.fail() && !source_filestream_.eof()) {
                auto msg = "Failed while reading file";
                BOOST_LOG_TRIVIAL(error) << msg;
                throw std::fstream::failure(msg);
            }		

            auto buf = boost::asio::buffer(buffer_.data(), static_cast<size_t>(source_filestream_.gcount()));
            write_buffer(buf);
		}
		else {
			std::stringstream ss;
			ss << "Successfully sent file " << path_ << " to server at " << endpoint_iterator_->host_name() << ":" << endpoint_iterator_->service_name();
			BOOST_LOG_TRIVIAL(trace) << ss.str();
			std::cout << ss.str() << std::endl;
			source_filestream_.close();

			tcp_routine();
		}
    } else {
        BOOST_LOG_TRIVIAL(error) << "Error: " << t_ec.message();
    }
}

void Client::handler_tcp_send_request(const boost::system::error_code & ec)
{
	if (!ec) {
		tcp_routine();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << "Error: " << ec.message();
	}
}

template<class Buffer>
void Client::write_buffer(Buffer& t_buffer)
{
	boost::asio::async_write(tcp_socket_,
		t_buffer,
		[this](boost::system::error_code ec, size_t length)
	{
		//std::cout << __FUNCTION__ << " Bytes written: " << length << std::endl;
		(*this.*current_write_func)(ec);
	});
}
