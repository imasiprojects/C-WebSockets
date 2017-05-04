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
    friend class NewWebSocketServer;

    ClientData* _clientData;

    std::mutex _dataPendingToBeSentMutex;
    std::list<std::string> _dataPendingToBeSent;

    NewWebSocketServer* _server;

    std::string _lastPingRequest;
    clock_t _lastPingRequestTime;

    bool _stopping;

    // WebSocket protocol data

    char _fragmentedDataOpCode;
    std::string _fragmentedData;

    void pong(const std::string& data);

public:

    NewWebSocketConnection(NewWebSocketServer* server, ClientData* clientData);
    NewWebSocketConnection(const NewWebSocketConnection&) = delete;
    virtual ~NewWebSocketConnection();

    void send(const std::string& key, const std::string& data);

    void ping(const std::string& pingData);

    void stop();

    bool isStopping() const;

    std::string getIp() const;
};

class NewWebSocketServer
{
    ThreadPool* _threadPool;
    TCPRawServer _server;

    std::mutex _rawTCPClientsMutex;
    std::list<ClientData*> _rawTCPClients;

    std::mutex _connectionsMutex;
    std::list<NewWebSocketConnection*> _connections;

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
    static void webSocketManagerTask(NewWebSocketServer*, std::list<NewWebSocketConnection*>::iterator);

public:
    NewWebSocketServer();
    NewWebSocketServer(const NewWebSocketServer&) = delete;
    NewWebSocketServer(NewWebSocketServer&&) = default;
    ~NewWebSocketServer();

    bool start(unsigned short port, size_t eventHandlerThreadCount);
    void stop();

    void sendBroadcast(const std::string& key, const std::string& data);
    void pingAll(const std::string& pingData);

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