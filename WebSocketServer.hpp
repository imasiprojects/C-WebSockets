#pragma once

#include <string>
#include <map>
#include <set>
#include <thread>

#include "sockets.hpp"

class WebSocketServer;
class WebSocketConnection;

typedef void(*WSNewClientCallback)(WebSocketServer* srv, WebSocketConnection* conn);
typedef void(*WSImasiCallback)(WebSocketServer* srv, WebSocketConnection* conn, std::string key, std::string data);


class WebSocketConnection{
    std::thread* _thread;

    WebSocketServer* _server;
    TCPClient _conn;

	bool _handShakeDone;
    bool _isRunning;
    bool _mustStop;

	std::string _lastPingRequest;
	clock_t _lastPingRequestTime;

    void threadFunction();
    bool performHandShake(std::string buffer);

public:
    WebSocketConnection(WebSocketServer* server, Connection conn);
    WebSocketConnection(const WebSocketConnection&) = delete;
    ~WebSocketConnection();

    std::string getIp() const;

    void send(std::string key, std::string data);

    void ping();
    void pong(std::string data);

    void stop();
    void stopAndWait();

    bool isRunning() const;
};

class WebSocketServer {

    TCPRawServer _server;

    std::set<WebSocketConnection*> _connections;

protected:
    WSNewClientCallback _onNewClient;
    WSImasiCallback _onUnknownMessage;

    /// IMASI Protocol
    std::map<std::string, WSImasiCallback> _messageCallbacks;

public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(unsigned short port);
    void close();

    bool isRunning() const;

    bool newClient();

    bool setNewClientCallback(WSNewClientCallback callback);

    /// IMASI Protocol
    bool setDataCallback(std::string key, WSImasiCallback callback);
    bool setUnknownMessageCallback(WSImasiCallback callback);

    void sendBroadcast(std::string key, std::string data);
	void sendPing();

    unsigned short getPort() const;

    WSNewClientCallback getNewClientCallback() const;
    WSImasiCallback getUnknownMessageCallback() const;
    const std::map<std::string, WSImasiCallback>& getMessageCallbacks() const;
};
