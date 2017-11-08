#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include <boost\filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "client.h"
#include "..\Common\nfd.h"

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

void choose_files(std::vector<std::string>& file_paths)
{
	nfdpathset_t outPaths;
	nfdresult_t result = NFD_OpenDialogMultiple(nullptr, nullptr, &outPaths);

	if (result == NFD_OKAY) {
		size_t curr_offset = 0;
		for (size_t i = 0; i < NFD_PathSet_GetCount(&outPaths); ++i) {
			file_paths.push_back(NFD_PathSet_GetPath(&outPaths, i));
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
    : m_ioService(t_ioService), m_socket(t_ioService), 
    m_endpointIterator(t_endpointIterator), m_path(t_path)
{
	routine();
}


void Client::openFile(std::string const &t_filepath)
{
	m_path = t_filepath;

	auto file_dir = m_path.substr(0, m_path.find_last_of('\\') + 1);
	auto file_name = m_path.substr(m_path.find_last_of('\\') + 1);
	auto new_file_name = replace_all(file_name, ' ', '_');
	auto new_file_path = file_dir + new_file_name;

	if (m_path != new_file_path) {
		boost::filesystem::rename(boost::filesystem::path(m_path), boost::filesystem::path(new_file_path));
		std::cout << "Renamed file to: " << new_file_name << std::endl;
		m_path = new_file_path;
	}

    m_sourceFile.open(m_path, std::ios_base::binary | std::ios_base::ate);
    if (m_sourceFile.fail())
        throw std::fstream::failure("Failed while opening file " + m_path);

    m_sourceFile.seekg(0, m_sourceFile.end);
    auto fileSize = m_sourceFile.tellg();
    m_sourceFile.seekg(0, m_sourceFile.beg);

    std::ostream requestStream(&m_request);
    boost::filesystem::path p(m_path);
    requestStream << p.filename().string() << "\n" << fileSize << "\n\n";
    BOOST_LOG_TRIVIAL(trace) << "Request size: " << m_request.size();
}

void Client::doConnect()
{
    boost::asio::async_connect(m_socket, m_endpointIterator, 
        [this](boost::system::error_code ec, TcpResolverIterator)
        {
            if (!ec) {
                writeBuffer(m_request);
            } else {
                std::cout << "Coudn't connect to host. Please run server "
                    "or check network connection.\n";
                BOOST_LOG_TRIVIAL(error) << "Error: " << ec.message();
            }
        });
}


void Client::doWriteFile(const boost::system::error_code& t_ec)
{
    if (!t_ec) {
        if (m_sourceFile) {
            m_sourceFile.read(m_buf.data(), m_buf.size()); 
            if (m_sourceFile.fail() && !m_sourceFile.eof()) {
                auto msg = "Failed while reading file";
                BOOST_LOG_TRIVIAL(error) << msg;
                throw std::fstream::failure(msg);
            }
            std::stringstream ss;
            ss << "Send " << m_sourceFile.gcount() << " bytes, total: "
                << m_sourceFile.tellg() << " bytes";
            BOOST_LOG_TRIVIAL(trace) << ss.str();
            std::cout << ss.str() << std::endl;

            auto buf = boost::asio::buffer(m_buf.data(), static_cast<size_t>(m_sourceFile.gcount()));
            writeBuffer(buf);
        }
    } else {
        BOOST_LOG_TRIVIAL(error) << "Error: " << t_ec.message();
    }
}

void Client::routine()
{	
	doConnect();
	openFile(m_path);
}

template<class Buffer>
void Client::writeBuffer(Buffer& t_buffer)
{
	boost::asio::async_write(m_socket,
		t_buffer,
		[this](boost::system::error_code ec, size_t /*length*/)
	{
		doWriteFile(ec);
	});
}
