#include <iostream>

#include <boost/asio/read_until.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "server.h"

void print_dir(std::string const& path, std::string const& präfix = ""){
	std::cout << präfix << char(200) << path.substr(path.find_last_of('\\')+1)<< '\\' << std::endl;
	
	boost::filesystem::directory_iterator end_itr;
	for (boost::filesystem::directory_iterator itr(path); itr != end_itr;)
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
			print_dir(curr_path.string(), "   " + präfix);
		}
	}
}

Session::Session(TcpSocket t_socket)
    : m_socket(std::move(t_socket))
{
}


void Session::doRead()
{
    auto self = shared_from_this();
    async_read_until(m_socket, m_requestBuf_, "\n\n",
        [this, self](boost::system::error_code ec, size_t bytes)
        {
            if (!ec)
                processRead(bytes);
            else
                handleError(__FUNCTION__, ec);
        });
}


void Session::processRead(size_t t_bytesTransferred)
{
    BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << "(" << t_bytesTransferred << ")" 
        << ", in_avail = " << m_requestBuf_.in_avail() << ", size = " 
        << m_requestBuf_.size() << ", max_size = " << m_requestBuf_.max_size() << ".";

    std::istream requestStream(&m_requestBuf_);
    readData(requestStream);

    auto pos = m_fileName.find_last_of('\\');
    if (pos != std::string::npos)
        m_fileName = m_fileName.substr(pos + 1);

    createFile();

    // write extra bytes to file
    do {
        requestStream.read(m_buf.data(), m_buf.size());
        //BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << " write " << requestStream.gcount() << " bytes.";
        m_outputFile.write(m_buf.data(), requestStream.gcount());
    } while (requestStream.gcount() > 0);

    auto self = shared_from_this();
    m_socket.async_read_some(boost::asio::buffer(m_buf.data(), m_buf.size()),
        [this, self](boost::system::error_code ec, size_t bytes)
        {
            if (!ec)
                doReadFileContent(bytes);
            else
                handleError(__FUNCTION__, ec);
        });
}


void Session::readData(std::istream &stream)
{
	std::string read_mode;
	stream >> read_mode;
	BOOST_LOG_TRIVIAL(trace) << "Received request of type: " << read_mode;
    stream >> m_fileName;
    stream >> m_fileSize;
    stream.read(m_buf.data(), 2);

    BOOST_LOG_TRIVIAL(trace) << m_fileName << " size is " << m_fileSize
        << ", tellg = " << stream.tellg();
}


void Session::createFile()
{
    m_outputFile.open(m_fileName, std::ios_base::binary);
    if (!m_outputFile) {
        BOOST_LOG_TRIVIAL(error) << __LINE__ << ": Failed to create: " << m_fileName;
        return;
    }
}


void Session::doReadFileContent(size_t t_bytesTransferred)
{
    if (t_bytesTransferred > 0) {
        m_outputFile.write(m_buf.data(), static_cast<std::streamsize>(t_bytesTransferred));

        BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << " recv " << m_outputFile.tellp() << " bytes";

        if (m_outputFile.tellp() >= static_cast<std::streamsize>(m_fileSize)) {
            std::cout << "Received file: " << m_fileName << std::endl;
            return;
        }
    }
    auto self = shared_from_this();
    m_socket.async_read_some(boost::asio::buffer(m_buf.data(), m_buf.size()),
        [this, self](boost::system::error_code ec, size_t bytes)
        {
            doReadFileContent(bytes);
        });
}


void Session::handleError(std::string const& t_functionName, boost::system::error_code const& t_ec)
{
    BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << " in " << t_functionName << " due to " 
        << t_ec << " " << t_ec.message() << std::endl;
}


Server::Server(IoService& t_ioService, short t_port, std::string const& t_workDirectory)
    : m_socket(t_ioService),
    m_acceptor(t_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), t_port)),
    m_workDirectory(t_workDirectory)
{
    std::cout << "Server started" << std::endl << std::endl;

    createWorkDirectory();

	std::cout << "Contents of working directory:" << std::endl;
	print_dir(m_workDirectory);

    doAccept();
}


void Server::doAccept()
{
    m_acceptor.async_accept(m_socket,
        [this](boost::system::error_code ec)
    {
        if (!ec)
            std::make_shared<Session>(std::move(m_socket))->start();

        doAccept();
    });
}


void Server::createWorkDirectory()
{
    using namespace boost::filesystem;
    auto currentPath = path(m_workDirectory);
    if (!exists(currentPath) && !create_directory(currentPath))
        BOOST_LOG_TRIVIAL(error) << "Coudn't create working directory: " << m_workDirectory;
    current_path(currentPath);
}
