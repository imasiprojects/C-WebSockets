#include "NewWebSocketServer.hpp"

#include "WebSocketHelper.hpp"
#include "HttpHelper.hpp"
#include "Sha1.hpp"
#include "Base64.hpp"


NewWebSocketConnection::NewWebSocketConnection(NewWebSocketServer* server, ClientData* clientData)
{
    _stopping = false;
    _server = server;
    _clientData = clientData;
}

NewWebSocketConnection::~NewWebSocketConnection()
{
}

void NewWebSocketConnection::send(std::string key, std::string data)
{
    _clientData->client->send(WebSocketHelper::mask((char) key.size() + key + data, 0x02));
}

void NewWebSocketConnection::stop()
{
    _stopping = true;
}

bool NewWebSocketConnection::isStopping() const
{
    return _stopping;
}


bool NewWebSocketServer::performHandshake(ClientData* clientData)
{
    std::string websocketKey = HttpHelper::getHeaderValue(clientData->buffer, "Sec-WebSocket-Key");

    if (websocketKey.size() > 0)
    {
        std::string sha1Key = Sha1::sha1UnsignedChar(websocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        std::string finalKey = Base64::encode(sha1Key.c_str());

        clientData->client->send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + finalKey + "\r\n\r\n");

        return true;
    }
    return false;
}

void NewWebSocketServer::acceptClientsTask(NewWebSocketServer* webSocketServer)
{
    TCPRawServer& server = webSocketServer->_server;

    while (webSocketServer->isRunning())
    {
        Connection connection = server.newClient();

        if (connection.sock != SOCKET_ERROR)
        {
            TCPClient* client = new TCPClient(connection.sock, connection.ip, server.getPort());

            webSocketServer->_rawTCPClientsMutex.lock();
            webSocketServer->_rawTCPClients.push_back(new ClientData{client, "", clock()});
            webSocketServer->_rawTCPClientsMutex.unlock();

            webSocketServer->_threadPool->addTask(NewWebSocketServer::acceptClientsTask, webSocketServer);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void NewWebSocketServer::handshakeHandlerTask(NewWebSocketServer* webSocketServer)
{
    if (webSocketServer->isRunning())
    {
        TCPRawServer& server = webSocketServer->_server;

        webSocketServer->_rawTCPClientsMutex.lock();

        if (webSocketServer->_rawTCPClients.size() == 0) // Should never happen
        {
            webSocketServer->_rawTCPClientsMutex.unlock();

            return;
        }

        ClientData* clientData = webSocketServer->_rawTCPClients.front();
        webSocketServer->_rawTCPClients.pop_front();
        webSocketServer->_rawTCPClientsMutex.unlock();

        TCPClient& client = *clientData->client;
        std::string& buffer = clientData->buffer;

        buffer += client.recv();

        if (!client.isConnected() || clock() - clientData->createdOn >= webSocketServer->getTimeout())
        {
            delete clientData->client;
            delete clientData;
        }
        else
        {
            size_t headerEndPosition = buffer.rfind("\r\n\r\n");

            if (headerEndPosition != std::string::npos)
            {
                bool _handShakeDone = performHandshake(clientData);

                if (_handShakeDone)
                {
                    clientData->buffer.clear();

                    WSInstantiator instantiator =  webSocketServer->getInstantiator();
                    WSEventCallback newClientCallback = webSocketServer->getNewClientCallback();

                    NewWebSocketConnection* connection;

                    if (instantiator != nullptr)
                    {
                        connection = instantiator(webSocketServer, clientData);
                    }
                    else
                    {
                        connection = new NewWebSocketConnection(webSocketServer, clientData);
                    }

                    if (newClientCallback != nullptr)
                    {
                        newClientCallback(webSocketServer, connection);
                    }

                    webSocketServer->_connectionsMutex.lock();
                    auto connectionIt = webSocketServer->_connections.insert(webSocketServer->_connections.end(), connection);
                    webSocketServer->_connectionsMutex.unlock();

                    webSocketServer->_threadPool->addTask(NewWebSocketServer::webSocketManagerTask, webSocketServer, connectionIt);
                }
                else
                {
                    // TODO: HTTP

                    delete clientData->client;
                    delete clientData;
                }
            }
            else
            {
                webSocketServer->_threadPool->addTask(NewWebSocketServer::acceptClientsTask, webSocketServer);
            }
        }
    }
}

void NewWebSocketServer::webSocketManagerTask(NewWebSocketServer* webSocketServer, std::list<NewWebSocketConnection*>::iterator connectionIt)
{
    NewWebSocketConnection* connection = *connectionIt;

    if (!connection->isStopping())
    {
        std::list<std::string> dataToBeSent;

        connection->_dataPendingToBeSentMutex.lock();
        connection->_dataPendingToBeSent.swap(dataToBeSent);
        connection->_dataPendingToBeSentMutex.unlock();

        for (auto data : dataToBeSent)
        {
            if (!connection->_clientData->client->send(data))
            {
                connection->stop();

                break;
            }
        }
    }

    // TODO: Receive data, save it into the buffer

    if (connection->isStopping())
    {
        delete connection->_clientData->client;
        delete connection->_clientData;
        delete connection;

        webSocketServer->_connectionsMutex.lock();
        webSocketServer->_connections.erase(connectionIt);
        webSocketServer->_connectionsMutex.unlock();
    }
    else
    {
        webSocketServer->_threadPool->addTask(NewWebSocketServer::webSocketManagerTask, webSocketServer, connectionIt);
    }
}


NewWebSocketServer::NewWebSocketServer()
    : _threadPool(nullptr)
    , _isStopping(false)
    , _acceptNewClients(true)
    , _timeout(1000)
{}

NewWebSocketServer::~NewWebSocketServer()
{
    stop();
}

bool NewWebSocketServer::start(unsigned short port, size_t eventHandlerThreadCount)
{
    stop();

    if (_server.start(port))
    {
        _server.setBlocking(false);

        _threadPool = new ThreadPool(eventHandlerThreadCount + 2);

        _threadPool->addTask(NewWebSocketServer::acceptClientsTask, this);
    }

    return false;
}

void NewWebSocketServer::stop()
{
    if (isRunning())
    {
        _isStopping = true;

        delete _threadPool;
        _threadPool = nullptr;
        _server.finish();

        for (auto clientData : _rawTCPClients)
        {
            delete clientData->client;
            delete clientData;
        }

        for (auto connection : _connections)
        {
            delete connection->_clientData->client;
            delete connection->_clientData;
            delete connection;
        }

        _isStopping = false;
    }
}

// Setters

void NewWebSocketServer::setTimeout(unsigned int milliseconds)
{
    _timeout = milliseconds;
}

void NewWebSocketServer::setAcceptNewClients(bool acceptNewClients)
{
    _acceptNewClients = acceptNewClients;
}

void NewWebSocketServer::setServeFolder(std::string serveFolder)
{
    _serveFolder = serveFolder;
}

void NewWebSocketServer::setDefaultPage(std::string defaultPage)
{
    _defaultPage = defaultPage;
}

void NewWebSocketServer::setInstantiator(WSInstantiator instantiator)
{
    _instantiator = instantiator;
}

void NewWebSocketServer::setDestructor(WSDestructor destructor)
{
    _destructor = destructor;
}

bool NewWebSocketServer::setNewClientCallback(WSEventCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onNewClient = callback;

    return true;
}

bool NewWebSocketServer::setClosedClientCallback(WSEventCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onClosedClient = callback;

    return true;
}

bool NewWebSocketServer::setUnknownMessageCallback(WSImasiCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onUnknownMessage = callback;

    return true;
}

bool NewWebSocketServer::setDataCallback(std::string key, WSImasiCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _messageCallbacks[key] = callback;

    return true;
}

// Getters

bool NewWebSocketServer::isRunning() const
{
    return _threadPool != nullptr && !_isStopping && _server.isOn();
}

bool NewWebSocketServer::isAcceptingNewClients() const
{
    return _acceptNewClients;
}

unsigned short NewWebSocketServer::getPort() const
{
    return _server.getPort();
}

unsigned int NewWebSocketServer::getTimeout() const
{
    return _timeout;
}

std::string NewWebSocketServer::getServeFolder() const
{
    return _serveFolder;
}

std::string NewWebSocketServer::getDefaultPage() const
{
    return _defaultPage;
}


WSInstantiator NewWebSocketServer::getInstantiator() const
{
    return _instantiator;
}

WSDestructor NewWebSocketServer::getDestructor() const
{
    return _destructor;
}

WSEventCallback NewWebSocketServer::getNewClientCallback() const
{
    return _onNewClient;
}

WSEventCallback NewWebSocketServer::getClosedClientCallback() const
{
    return _onClosedClient;
}

WSImasiCallback NewWebSocketServer::getUnknownMessageCallback() const
{
    return _onUnknownMessage;
}

const std::map<std::string, WSImasiCallback>& NewWebSocketServer::getMessageCallbacks() const
{
    return _messageCallbacks;
}