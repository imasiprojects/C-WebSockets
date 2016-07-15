#pragma once

#include <string>
#include <map>
#include <list>
#include <thread>

#include "sockets.hpp"

class WebSocketServer;

typedef void(*WSCallback)(WebSocketServer& srv, std::string data);
typedef void(*WSImasiCallback)(WebSocketServer& srv, std::string data);

class WebSocketServer {
    TCPRawServer _server;

    /// Native
    WSCallback _onNewClient;
    WSCallback _onUnknownClientMessage;

    /// IMASI Protocol
    std::map<std::string, WSImasiCallback> _clientMsg;


    std::list<std::thread*> _clientThreads;
    static void clientThreadFunction(WebSocketServer& srv, Connection conn);

public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(unsigned short port);
    void close();

    bool newClient();

    /// Native
    void setDataCallback(WSCallback callback);
    void setNewClientCallback(WSCallback callback);

    bool send(std::string data);
    bool sendBroadcast(std::string data);


    /// IMASI Protocol
    void setDataCallback(std::string key, WSImasiCallback callback);

    bool send(std::string key, std::string data);
    bool sendBroadcast(std::string key, std::string data);

};
