#pragma once

#include <string>
#include <vector>
#include <winsock2.h>

struct Connection {
	SOCKET sock;
	std::string ip;

	Connection() :sock(INVALID_SOCKET) {}
};

typedef void(TCPSocketCallback)(Connection c);

std::basic_string<char> recv(SOCKET s, size_t maxChars = 1024);

bool send(SOCKET s, std::string msg);

void setBlocking(SOCKET sock, bool blocking);

Connection getNewClient(unsigned short port);

class TCPRawServer {
	SOCKET _listener;
	unsigned short _port;
	bool _blocking;
	bool _on;

public:
	TCPRawServer();
	TCPRawServer(unsigned short port);
	~TCPRawServer();
	bool start(unsigned short port);
	void finish();
	Connection newClient() const;
	bool newClient(TCPSocketCallback* callback, bool detach = true) const;
	bool isOn()const;
	unsigned short getPort()const;
	void setBlocking(bool blocking);
	bool isBlocking()const;
};

class TCPServer :public TCPRawServer {
	struct _client {
		SOCKET socket;
		std::string ip;
		bool blocking;
		std::vector<std::string> data;

		_client() :socket(INVALID_SOCKET), blocking(0) {}
	};

	std::vector<_client> _clients;

public:
	TCPServer();
	TCPServer(unsigned short port);
	~TCPServer();
	bool newClient();
	void disconnectClient(size_t clientN);
	std::string recv(size_t clientN, size_t maxChars = 1024);
	bool send(size_t clientN, std::string msg);
	std::vector<std::string>* getData(size_t clientN);
	std::string getIp(size_t clientN)const;
	void setBlocking(size_t clientN, bool blocking);
	bool isBlocking(size_t clientN)const;
	size_t getClientCount()const;

};

class TCPClient {
	SOCKET _socket;
	std::string _ip;
	unsigned short _port;
	bool _connected;
	bool _blocking;

	std::vector<char> _recvBuffer;

public:
	TCPClient();
	TCPClient(std::string ip, unsigned short port);
	~TCPClient();
	bool connect(std::string ip, unsigned short port);
	void connect(SOCKET sock, std::string ip, unsigned short port);
	void disconnect();
	std::string recv(int maxChars = 1024);
	bool send(const std::string& msg);
	bool isConnected()const;
	std::string getIp()const;
	unsigned short getPort()const;
	void setBlocking(bool blocking);
	bool isBlocking()const;
};
