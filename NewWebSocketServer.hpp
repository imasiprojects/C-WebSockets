#ifndef WEBSOCKETSERVER_HPP
#define WEBSOCKETSERVER_HPP

#include <map>
#include <list>
#include <thread>

#include "sockets.hpp"
#include "ThreadPool.hpp"

// Temporal
class WebSocketConnection;
class NewWebSocketServer;

using WSEventCallback = void(*)(NewWebSocketServer* server, WebSocketConnection* conn);
using WSImasiCallback = void(*)(NewWebSocketServer* server, WebSocketConnection* conn, std::string key, std::string data);
using WSInstantiator = WebSocketConnection*(*)(NewWebSocketServer* server, Connection conn);
using WSDestructor = void(*)(NewWebSocketServer* server, WebSocketConnection* conn);



class NewWebSocketServer
{
    ThreadPool* _threadPool;
    TCPRawServer _server;

    bool _isStopping;
    bool _acceptNewClients;

    std::string _serveFolder;
    std::string _defaultPage;

    // Callbacks

    WSInstantiator _instantiator;
    WSDestructor _destructor;

    WSEventCallback _onNewClient;
    WSEventCallback _onClosedClient;

    WSImasiCallback _onUnknownMessage;
    std::map<std::string, WSImasiCallback> _messageCallbacks;

    // Tasks

    static void acceptClientsTask();

public:
    NewWebSocketServer();
    ~NewWebSocketServer();

    bool start(unsigned short port, size_t eventHandlerThreadCount);
    void stop();


    //bool acceptNewClient(); TASK
    //bool clearClosedConnections(); MADE WHILE READING

    void sendBroadcast(std::string key, std::string data);
    void sendPing();

    // Setters

    void setAcceptNewClients(bool acceptNewClients);

    void setServeFolder(std::string serveFolder);
    void setDefaultPage(std::string defaultPage);

    void setInstantiator(WSInstantiator instantiator);
    void setDestructor(WSDestructor destructor);

    bool setNewClientCallback(WSEventCallback callback);
    bool setClosedClientCallback(WSEventCallback callback);

    bool setUnknownMessageCallback(WSImasiCallback callback);
    bool setDataCallback(std::string key, WSImasiCallback callback);

    // Getters

    bool isRunning() const;
    bool isAcceptingNewClients() const;
    unsigned short getPort() const;

    std::string getServeFolder() const;
    std::string getDefaultPage() const;

    WSInstantiator getInstantiator() const;
    WSDestructor getDestructor() const;

    WSEventCallback getNewClientCallback() const;
    WSEventCallback getClosedClientCallback() const;

    WSImasiCallback getUnknownMessageCallback() const;
    const std::map<std::string, WSImasiCallback>& getMessageCallbacks() const;
};


#endif