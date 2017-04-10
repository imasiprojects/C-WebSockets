#pragma once

#include <string>
#include <map>
#include <list>
#include <thread>

#include "sockets.hpp"

class WebSocketServer;
class WebSocketConnection;

using WSEventCallback = void(*)(WebSocketServer* srv, WebSocketConnection* conn);
using WSImasiCallback = void(*)(WebSocketServer* srv, WebSocketConnection* conn, std::string key, std::string data);
using WSInstantiator = WebSocketConnection*(*)(WebSocketServer* server, Connection conn);


class WebSocketConnection
{
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
    virtual ~WebSocketConnection();

    std::string getIp() const;

    void send(std::string key, std::string data);

    void ping();
    void pong(std::string data);

    void stop();
    void stopAndWait();

    bool isRunning() const;
};

class WebSocketServer
{

    TCPRawServer _server;

    bool _acceptNewClients;

    std::list<WebSocketConnection*> _connections;

protected:
    std::string _serveFolder;
    std::string _defaultPage;

    WSInstantiator _instantiator;

    WSEventCallback _onNewClient;
    WSEventCallback _onClosedClient;
    WSImasiCallback _onUnknownMessage;

    /// IMASI Protocol
    std::map<std::string, WSImasiCallback> _messageCallbacks;

public:
    WebSocketServer();
    virtual ~WebSocketServer();

    bool start(unsigned short port);
    bool startAndWait(unsigned short port);
    void close();

    bool isRunning() const;

    bool acceptNewClient();
    bool clearClosedConnections();

    bool setNewClientCallback(WSEventCallback callback);
    bool setClosedClientCallback(WSEventCallback callback);

    /// IMASI Protocol
    bool setDataCallback(std::string key, WSImasiCallback callback);
    bool setUnknownMessageCallback(WSImasiCallback callback);

    void sendBroadcast(std::string key, std::string data);
    void sendPing();

    unsigned short getPort() const;

    void setAcceptNewClients(bool acceptNewClients);
    bool isAcceptingNewClients() const;

    void setServeFolder(std::string serveFolder);
    std::string getServeFolder() const;

    void setDefaultPage(std::string defaultPage);
    std::string getDefaultPage() const;

    void setInstantiator(WSInstantiator instantiator);

    WSEventCallback getNewClientCallback() const;
    WSEventCallback getClosedClientCallback() const;
    WSImasiCallback getUnknownMessageCallback() const;
    const std::map<std::string, WSImasiCallback>& getMessageCallbacks() const;
};
