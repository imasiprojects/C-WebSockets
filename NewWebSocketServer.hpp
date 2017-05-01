#ifndef WEBSOCKETSERVER_HPP
#define WEBSOCKETSERVER_HPP

#include <map>
#include <list>
#include <utility>
#include <mutex>
#include <thread>
#include <ctime>

#include "sockets.hpp"
#include "ThreadPool.hpp"

// Temporal
struct ClientData;
class NewWebSocketConnection;
class NewWebSocketServer;

using WSEventCallback = void(*)(NewWebSocketServer* server, NewWebSocketConnection* conn);
using WSImasiCallback = void(*)(NewWebSocketServer* server, NewWebSocketConnection* conn, std::string key, std::string data);
using WSInstantiator = NewWebSocketConnection*(*)(NewWebSocketServer* server, ClientData* clientData);
using WSDestructor = void(*)(NewWebSocketServer* server, NewWebSocketConnection* conn);


struct ClientData
{
    TCPClient* client;
    std::string buffer;
    clock_t createdOn;
};

class NewWebSocketConnection
{
    ClientData* _clientData;

    NewWebSocketServer* _server;

    std::string _lastPingRequest;
    clock_t _lastPingRequestTime;

public:
    NewWebSocketConnection(NewWebSocketServer* server, ClientData* clientData);
    NewWebSocketConnection(const NewWebSocketConnection&) = delete;
    virtual ~NewWebSocketConnection();

    std::string getIp() const;

    void send(std::string key, std::string data);

    void ping();
    void pong(std::string data);
};

class NewWebSocketServer
{
    ThreadPool* _threadPool;
    TCPRawServer _server;

    std::mutex _rawTCPClientsMutex;
    std::list<ClientData*> _rawTCPClients;

    bool _isStopping;
    bool _acceptNewClients;

    std::string _serveFolder;
    std::string _defaultPage;

    unsigned int _timeout;

    // Callbacks

    WSInstantiator _instantiator;
    WSDestructor _destructor;

    WSEventCallback _onNewClient;
    WSEventCallback _onClosedClient;

    WSImasiCallback _onUnknownMessage;
    std::map<std::string, WSImasiCallback> _messageCallbacks;

    // Helpers

    static bool performHandshake(ClientData*);

    // Tasks

    static void acceptClientsTask(NewWebSocketServer*);
    static void handshakeHandlerTask(NewWebSocketServer*);

public:
    NewWebSocketServer();
    NewWebSocketServer(const NewWebSocketServer&) = delete;
    NewWebSocketServer(NewWebSocketServer&&) = default;
    ~NewWebSocketServer();

    bool start(unsigned short port, size_t eventHandlerThreadCount);
    void stop();


    //bool acceptNewClient(); TASK
    //bool clearClosedConnections(); MADE WHILE READING

    void sendBroadcast(std::string key, std::string data);
    void sendPing();

    // Setters

    void setTimeout(unsigned int ms);

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

    unsigned int getTimeout() const;

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