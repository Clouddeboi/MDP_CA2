#pragma once
class Server
{
public:
	Server(UDPSocketPtr server_socket, string server_address);
	void DoServiceLoop();

	void ProcessReceivedData(char* receive_buffer, int bytes_received, const SocketAddress& sender_address, int requests, bool service_running);
	std::string ReturnCurrentDateAndTime();
	void ReceivePlayerInputByteStream(const UDPSocketPtr server_socket);
	void ReceivePlayerInputBitStream(const UDPSocketPtr server_socket);

private:

private:
	UDPSocketPtr server_socket_;
};

