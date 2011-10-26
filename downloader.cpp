
#include "downloader.h"


void Downloader::init(std::string _host, std::string _port, std::string _starting_uri_dir)
{
		host = _host;
		port = _port;
		starting_uri_dir = _starting_uri_dir;
}

void Downloader::run()
{	
		while(!files_to_download.empty()) {
				for(size_t i = 0; i < files_to_download.size(); ++i) {
						_download(files_to_download.front());
						files_to_download.pop_front();
				}
		}
}

void Downloader::get(std::string uri, std::string expected_checksum, int tries) 
{
		files_to_download.push_back(FileToDownload(uri, expected_checksum, tries));
}

void Downloader::_download(FileToDownload file)
{
		if(file.tries >= MAX_TRIES) {
				std::cout << "Error - Could not download " << file.uri << std::endl;
				return;	
		}

		std::cout << "Downloading " << file.uri << std::endl;
		int last_slash = file.uri.find_last_of('/');
		std::string local_dir = file.uri.substr(0, last_slash);
		std::string filename = file.uri.substr(last_slash + 1);

		// Find host using boost
		tcp::resolver resolver(io_service);
    tcp::resolver::query query(tcp::v4(), host, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		tcp::socket socket(io_service);
		error = boost::asio::error::host_not_found;
	
		// Connect socket to host
		while(error && endpoint_iterator != end) {
	    	socket.close();
	    	socket.connect(*endpoint_iterator++, error);
		}

		if(error) {
		    throw boost::system::system_error(error);
		}

		// Build request
	  std::stringstream request;
	  request << "GET " + starting_uri_dir + file.uri + " HTTP/1.0 \r\n";
	  request << "Host: " << host << "\r\n";
	  request << "\r\n";

	 	// Make request
		boost::system::error_code ignored_error;
		boost::asio::write(socket, boost::asio::buffer(request.str()), boost::asio::transfer_all(), ignored_error);

		// Read headers
		boost::asio::streambuf response_buf;
		std::stringstream response_stream;
    boost::asio::read_until(socket, response_buf, "\r\n\r\n");

    // Read content
    std::string response = "";
    while(boost::asio::read(socket, response_buf, boost::asio::transfer_at_least(1), error)) {
      	response_stream << &response_buf;
    }

		// Parse out the headers
		response = response_stream.str();
		response = response_stream.str().substr(response.find("\r\n\r\n") + 4);

		// Make the temporary directories and then save the file
		FileManager::mkdirs("tmp" + local_dir);
		std::string tmp_path = "tmp" + file.uri;
		FileManager::write(response, tmp_path);

		std::string actual_checmsum = FileManager::checksum(tmp_path);

		// If we can check for file integrity, do so here and short circuit if necessary
		if(!file.expected_checksum.empty()) {
				if(file.expected_checksum != actual_checmsum) {
						// If the checksum does not match, requeue
						get(file.uri, file.expected_checksum, file.tries + 1);
						return;
				}
		}

		// Move the downloaded file to its correct location
		local_dir = "./data" + local_dir + "/";
		FileManager::mkdirs(local_dir);
		FileManager::mv(tmp_path, local_dir + filename);
}
