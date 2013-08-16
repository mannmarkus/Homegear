#include "CLIClient.h"
#include "../GD.h"

namespace CLI {

Client::Client()
{

}

Client::~Client()
{
	try
	{
		_stopPingThread = true;
		if(_pingThread.joinable()) _pingThread.join();
	}
    catch(const std::exception& ex)
    {
    	std::cerr << "Error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ <<": " << ex.what() << std::endl;
    }
    catch(const Exception& ex)
    {
    	std::cerr << "Error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ <<": " << ex.what() << std::endl;
    }
    catch(...)
    {
    	std::cerr << "Unknown error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << "." << std::endl;
    }
}

void Client::ping()
{
	try
	{
		char buffer[1] = {0};
		while(!_stopPingThread)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			_sendMutex.lock();
			if(_closed) return;
			if(send(_fileDescriptor, buffer, 1, MSG_NOSIGNAL) == -1)
			{
				_closed = true;
				std::cout << std::endl << "Connection closed." << std::endl;
				_sendMutex.unlock();
				return;
			}
			_sendMutex.unlock();
		}
	}
    catch(const std::exception& ex)
    {
    	_sendMutex.unlock();
    	std::cerr << "Error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ <<": " << ex.what() << std::endl;
    }
    catch(const Exception& ex)
    {
    	_sendMutex.unlock();
    	std::cerr << "Error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ <<": " << ex.what() << std::endl;
    }
    catch(...)
    {
    	_sendMutex.unlock();
    	std::cerr << "Unknown error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << "." << std::endl;
    }
}

void Client::start()
{
	try
	{
		int32_t fileDescriptor = socket(AF_UNIX, SOCK_STREAM, 0);
		_fileDescriptor = fileDescriptor;
		if(fileDescriptor == -1)
		{
			std::cerr << "Could not create socket." << std::endl;
			return;
		}
		if(GD::debugLevel >= 4) std::cout << "Info: Trying to connect..." << std::endl;
		sockaddr_un remoteAddress;
		remoteAddress.sun_family = AF_UNIX;
		strcpy(remoteAddress.sun_path, GD::socketPath.c_str());
		if(connect(fileDescriptor, (struct sockaddr*)&remoteAddress, strlen(remoteAddress.sun_path) + sizeof(remoteAddress.sun_family)) == -1)
		{
			std::cerr << "Could not connect to socket. Error: " << strerror(errno) << std::endl;
			return;
		}
		if(GD::debugLevel >= 4) std::cout << "Info: Connected." << std::endl;

		_pingThread = std::thread(&CLI::Client::ping, this);

		rl_bind_key('\t', rl_abort); //no autocompletion

		std::string lastCommand;
		std::string currentCommand;
		char* sendBuffer;
		char receiveBuffer[1025];
		uint32_t bytes;
		while((sendBuffer = readline("> ")) != NULL)
		{
			if(_closed) break;
			bytes = strlen(sendBuffer);
			if(sendBuffer[0] == '\n' || sendBuffer[0] == 0) continue;
			if(strcmp(sendBuffer, "quit") == 0 || strcmp(sendBuffer, "exit") == 0)
			{
				_closed = true;
				close(fileDescriptor);
				free(sendBuffer);
				return;
			}
			_sendMutex.lock();
			if(_closed) break;
			if(send(fileDescriptor, sendBuffer, bytes, MSG_NOSIGNAL) == -1)
			{
				std::cerr << "Error sending to socket." << std::endl;
				close(fileDescriptor);
				free(sendBuffer);
				return;
			}
			currentCommand = std::string(sendBuffer);
			if(currentCommand != lastCommand)
			{
				lastCommand = currentCommand;
				add_history(sendBuffer); //Sets sendBuffer to 0
			}
			else
			{
				free(sendBuffer);
				sendBuffer = 0;
			}

			while(true)
			{
				if((bytes = recv(fileDescriptor, receiveBuffer, 1024, 0)) > 0)
				{
					receiveBuffer[bytes] = 0;
					std::cout << receiveBuffer;
					if(bytes < 1024 || (bytes == 1024 && receiveBuffer[bytes - 1] == 0)) break;
				}
				else
				{
					if(bytes < 0) std::cerr << "Error receiving data from socket." << std::endl;
					else std::cout << "Connection closed." << std::endl;
					close(fileDescriptor);
					return;
				}
			}
			_sendMutex.unlock();
		}
	}
    catch(const std::exception& ex)
    {
    	std::cerr << "Couldn't create socket file " << GD::socketPath << ": " << ex.what() << std::endl;
    }
    catch(const Exception& ex)
    {
    	std::cerr << "Error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ <<": " << ex.what() << std::endl;
    }
    catch(...)
    {
    	std::cerr << "Unknown error in file " << __FILE__ " line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << "." << std::endl;
    }
}

} /* namespace CLI */
