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
    _stopping = false;
    _lastPingRequestTime = 0;
    _fragmentedDataOpCode = 0;
}

NewWebSocketConnection::~NewWebSocketConnection()
{
}

void NewWebSocketConnection::send(const std::string& key, const std::string& data)
{
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask((char) key.size() + key + data, 0x02));
    _dataPendingToBeSentMutex.unlock();
}

void NewWebSocketConnection::ping(const std::string& pingData)
{
    _lastPingRequest = pingData;
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask(_lastPingRequest, 0x9));
    _dataPendingToBeSentMutex.unlock();
    _lastPingRequestTime = clock();
}

void NewWebSocketConnection::pong(const std::string& pingData)
{
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask(pingData, 0xA));
    _dataPendingToBeSentMutex.unlock();
}

void NewWebSocketConnection::stop()
{
    _stopping = true;
}

bool NewWebSocketConnection::isStopping() const
{
    return _stopping;
}

std::string NewWebSocketConnection::getIp() const
{
    return _clientData->client->getIp();
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

            webSocketServer->_threadPool->addTask(NewWebSocketServer::handshakeHandlerTask, webSocketServer);
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

    if (!connection->isStopping())
    {
        std::string& buffer = connection->_clientData->buffer;

        buffer += connection->_clientData->client->recv();

        if (buffer.size() >= 2)
        {
            bool fin = buffer[0] & 0x80;
            char opCode = buffer[0] & 0xF;
            bool hasMask = buffer[1] & 0x80;
            uint64_t packetSize = buffer[1] & 0x7F;
            char packetSizeLength = 1;

            bool continueProtocol = true;

            if (packetSize == 126)
            {
                packetSizeLength = 2;

                if (buffer.size() >= 4)
                {
                    packetSize = *(uint16_t*) &buffer[2];
                }
                else
                {
                    continueProtocol = false;
                }
            }
            else if (packetSize == 127)
            {
                packetSizeLength = 8;

                if (buffer.size() >= 10)
                {
                    packetSize = *(uint64_t*) &buffer[2];
                }
                else
                {
                    continueProtocol = false;
                }
            }

            if (continueProtocol && buffer.size() >= 1 + packetSizeLength + hasMask*4 + packetSize)
            {
                std::string data;

                if (hasMask)
                {
                    std::string mask = buffer.substr(1 + packetSizeLength, 4);
                    data = WebSocketHelper::unmask(mask, buffer.substr(1 + packetSizeLength + 4, packetSize));
                }
                else
                {
                    data = buffer.substr(1 + packetSizeLength, packetSize);
                }

                buffer.erase(0, 1 + packetSizeLength + hasMask * 4 + packetSize);

                if (fin)
                {
                    if (opCode == 0x0)
                    {
                        opCode = connection->_fragmentedDataOpCode;
                        data = connection->_fragmentedData + data;

                        connection->_fragmentedDataOpCode = 0;
                        connection->_fragmentedData.clear();
                    }

                    switch (opCode)
                    {
                        case 0x8: // Connection closed
                        {
                            connection->stop();
                            break;
                        }

                        case 0x9: // Ping
                        {
                            connection->pong(data);
                            break;
                        }

                        case 0xA: // Pong
                        {
                            if (data == connection->_lastPingRequest)
                            {
                                connection->_lastPingRequestTime = 0;
                            }

                            break;
                        }

                        default:
                        {
                            std::string key = data.substr(1, data[0]);
                            std::string value = data.substr(data[0] + 1);

                            const std::map<std::string, WSImasiCallback>& dataCallBacks = webSocketServer->getMessageCallbacks();
                            auto it = dataCallBacks.find(key);

                            if (it != dataCallBacks.end())
                            {
                                it->second(webSocketServer, connection, key, value);
                            }
                            else
                            {
                                WSImasiCallback unknownDataCallback = webSocketServer->getUnknownMessageCallback();

                                if (unknownDataCallback != nullptr)
                                {
                                    unknownDataCallback(webSocketServer, connection, key, value);
                                }
                            }

                            break;
                        }
                    }
                }
                else
                {
                    if (opCode != 0x0)
                    {
                        connection->_fragmentedDataOpCode = opCode;
                    }

                    connection->_fragmentedData += data;
                }

            }
        }
    }

    if (connection->_lastPingRequestTime != 0 &&
        clock() - connection->_lastPingRequestTime > webSocketServer->_timeout)
    {
        connection->stop();
    }

    if (connection->isStopping())
    {
        auto closedClientCallback = webSocketServer->getClosedClientCallback();

        if (closedClientCallback != nullptr)
        {
            closedClientCallback(webSocketServer, connection);
        }

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

        return true;
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

void NewWebSocketServer::sendBroadcast(const std::string& key, const std::string& data)
{
    _connectionsMutex.lock();

    std::string rawData = WebSocketHelper::mask((char) key.size() + key + data, 0x02);

    for (auto connection : _connections)
    {
        connection->_dataPendingToBeSentMutex.lock();
        connection->_dataPendingToBeSent.emplace_back(rawData);
        connection->_dataPendingToBeSentMutex.unlock();
    }

    _connectionsMutex.unlock();
}

void NewWebSocketServer::pingAll(const std::string& pingData)
{
    _connectionsMutex.lock();

    for (auto connection : _connections)
    {
        connection->ping(pingData);
    }

    _connectionsMutex.unlock();
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