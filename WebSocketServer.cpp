#include "WebSocketServer.hpp"

#include "WebSocketHelper.hpp"
#include "HttpHelper.hpp"
#include "Sha1.hpp"
#include "Base64.hpp"


WebSocketConnection::WebSocketConnection(WebSocketServer* server, ClientData* clientData)
{
    _stopping = false;
    _server = server;
    _clientData = clientData;
    _isWaitingPingRequestResponse = false;
    _stopping = false;
    _fragmentedDataOpCode = 0;
}

WebSocketConnection::~WebSocketConnection()
{
}

void WebSocketConnection::send(const std::string& key, const std::string& data)
{
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask((char) key.size() + key + data, 0x02));
    _dataPendingToBeSentMutex.unlock();
}

void WebSocketConnection::ping(const std::string& pingData)
{
    _lastPingRequest = pingData;
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask(_lastPingRequest, 0x9));
    _dataPendingToBeSentMutex.unlock();
    _lastPingRequestTime = std::chrono::steady_clock::now();
    _isWaitingPingRequestResponse = true;
}

void WebSocketConnection::pong(const std::string& pingData)
{
    _dataPendingToBeSentMutex.lock();
    _dataPendingToBeSent.emplace_back(WebSocketHelper::mask(pingData, 0xA));
    _dataPendingToBeSentMutex.unlock();
}

void WebSocketConnection::stop()
{
    _stopping = true;
}

bool WebSocketConnection::isStopping() const
{
    return _stopping;
}

std::string WebSocketConnection::getIp() const
{
    return _clientData->client->getIp();
}

bool WebSocketServer::performHandshake(ClientData* clientData)
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

void WebSocketServer::acceptClientsTask(WebSocketServer* webSocketServer)
{
    TCPRawServer& server = webSocketServer->_server;

    while (webSocketServer->isRunning())
    {
        Connection connection = server.newClient();

        if (connection.sock != SOCKET_ERROR)
        {
            TCPClient* client = new TCPClient(connection.sock, connection.ip, server.getPort());

            webSocketServer->_rawTCPClientsMutex.lock();
            webSocketServer->_rawTCPClients.push_back(new ClientData{client, "", std::chrono::steady_clock::now()});
            webSocketServer->_rawTCPClientsMutex.unlock();

            webSocketServer->_threadPool->addTask(WebSocketServer::handshakeHandlerTask, webSocketServer);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void WebSocketServer::handshakeHandlerTask(WebSocketServer* webSocketServer)
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

        if (!client.isConnected() || std::chrono::steady_clock::now() - clientData->createdOn >= webSocketServer->getTimeout())
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

                    WebSocketConnection* connection;

                    if (instantiator != nullptr)
                    {
                        connection = instantiator(webSocketServer, clientData);
                    }
                    else
                    {
                        connection = new WebSocketConnection(webSocketServer, clientData);
                    }

                    if (newClientCallback != nullptr)
                    {
                        newClientCallback(webSocketServer, connection);
                    }

                    webSocketServer->_connectionsMutex.lock();
                    auto connectionIt = webSocketServer->_connections.insert(webSocketServer->_connections.end(), connection);
                    webSocketServer->_connectionsMutex.unlock();

                    webSocketServer->_threadPool->addTask(WebSocketServer::webSocketManagerTask, webSocketServer, connectionIt);
                }
                else
                {
                    /// HTTP Server

                    std::string folder = webSocketServer->getServeFolder();

                    if (folder != "")
                    {
                        std::string request = buffer.substr(0, buffer.find("HTTP") - 1);
                        request = request.substr(request.find(" ") + 1);
                        request = folder + request;

                        if (request.size() == 0
                            || request[request.size() - 1] == '/'
                            || request[request.size() - 1] == '\\')
                        {
                            request += webSocketServer->getDefaultPage();
                        }

                        int leftTrimLength = 0;

                        while (request[leftTrimLength] == '/' || request[leftTrimLength] == '\\')
                        {
                            ++leftTrimLength;
                        }

                        std::ifstream file(request.substr(leftTrimLength), std::ios::binary | std::ios::ate);

                        if (!file)
                        {
                            client.send("HTTP/1.1 404 NOT FOUND\r\ncontent-length:14\r\nconnection: close\r\n\r\nFile not found");
                        }
                        else
                        {
                            size_t size = file.tellg();
                            char* buffer = new char[4096];
                            client.send("HTTP/1.1 200 OK\r\nconnection: close\r\ncontent-length:" + std::to_string(size) + "\r\n\r\n");
                            file.seekg(0);

                            while (size > 0)
                            {
                                file.read(buffer, 4096);
                                size_t readed = file.gcount();

                                if (readed > 0)
                                {
                                    client.send(std::string(buffer, readed));
                                    size -= readed;
                                }
                            }

                            delete[] buffer;
                        }

                    }

                    delete clientData->client;
                    delete clientData;
                }
            }
            else
            {
                webSocketServer->_rawTCPClientsMutex.lock();
                webSocketServer->_rawTCPClients.emplace_back(clientData);
                webSocketServer->_rawTCPClientsMutex.unlock();

                webSocketServer->_threadPool->addTask(WebSocketServer::handshakeHandlerTask, webSocketServer);
            }
        }
    }
}

void WebSocketServer::webSocketManagerTask(WebSocketServer* webSocketServer, std::list<WebSocketConnection*>::iterator connectionIt)
{
    WebSocketConnection* connection = *connectionIt;

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

        if (!connection->_clientData->client->isConnected())
        {
            connection->stop();
        }
        else if (buffer.size() >= 2)
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
                                connection->_isWaitingPingRequestResponse = false;
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

    if (connection->_isWaitingPingRequestResponse &&
        std::chrono::steady_clock::now() - connection->_lastPingRequestTime > webSocketServer->getTimeout())
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

        WSDestructor destructorCallback = webSocketServer->getDestructor();

        if (destructorCallback != nullptr)
        {
            destructorCallback(webSocketServer, connection);
        }
        else
        {
            delete connection;
        }

        webSocketServer->_connectionsMutex.lock();
        webSocketServer->_connections.erase(connectionIt);
        webSocketServer->_connectionsMutex.unlock();
    }
    else
    {
        webSocketServer->_threadPool->addTask(WebSocketServer::webSocketManagerTask, webSocketServer, connectionIt);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}


WebSocketServer::WebSocketServer()
    : _threadPool(nullptr)
    , _isStopping(false)
    , _acceptNewClients(true)
    , _timeout(1000)
{}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(unsigned short port, size_t eventHandlerThreadCount)
{
    stop();

    if (_server.start(port))
    {
        _server.setBlocking(false);

        _threadPool = new ThreadPool(eventHandlerThreadCount + 2);

        _threadPool->addTask(WebSocketServer::acceptClientsTask, this);

        return true;
    }

    return false;
}

void WebSocketServer::stop()
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

void WebSocketServer::sendBroadcast(const std::string& key, const std::string& data)
{
    sendBroadcastExcluding(nullptr, key, data);
}

void WebSocketServer::sendBroadcastExcluding(WebSocketConnection* excludedConnection, const std::string& key, const std::string& data)
{
    std::string rawData = WebSocketHelper::mask((char) key.size() + key + data, 0x02);

    _connectionsMutex.lock();

    for (WebSocketConnection* connection : _connections)
    {
        if (connection != excludedConnection)
        {
            connection->_dataPendingToBeSentMutex.lock();
            connection->_dataPendingToBeSent.emplace_back(rawData);
            connection->_dataPendingToBeSentMutex.unlock();
        }
    }

    _connectionsMutex.unlock();
}

void WebSocketServer::pingAll(const std::string& pingData)
{
    _connectionsMutex.lock();

    for (auto connection : _connections)
    {
        connection->ping(pingData);
    }

    _connectionsMutex.unlock();
}

// Setters

void WebSocketServer::setTimeout(unsigned int milliseconds)
{
    _timeout = std::chrono::milliseconds(milliseconds);
}

void WebSocketServer::setAcceptNewClients(bool acceptNewClients)
{
    _acceptNewClients = acceptNewClients;
}

void WebSocketServer::setServeFolder(std::string serveFolder)
{
    _serveFolder = serveFolder;
}

void WebSocketServer::setDefaultPage(std::string defaultPage)
{
    _defaultPage = defaultPage;
}

void WebSocketServer::setInstantiator(WSInstantiator instantiator)
{
    _instantiator = instantiator;
}

void WebSocketServer::setDestructor(WSDestructor destructor)
{
    _destructor = destructor;
}

bool WebSocketServer::setNewClientCallback(WSEventCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onNewClient = callback;

    return true;
}

bool WebSocketServer::setClosedClientCallback(WSEventCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onClosedClient = callback;

    return true;
}

bool WebSocketServer::setUnknownMessageCallback(WSImasiCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _onUnknownMessage = callback;

    return true;
}

bool WebSocketServer::setDataCallback(std::string key, WSImasiCallback callback)
{
    if (isRunning())
    {
        return false;
    }

    _messageCallbacks[key] = callback;

    return true;
}

// Getters

bool WebSocketServer::isRunning() const
{
    return _threadPool != nullptr && !_isStopping && _server.isOn();
}

bool WebSocketServer::isAcceptingNewClients() const
{
    return _acceptNewClients;
}

unsigned short WebSocketServer::getPort() const
{
    return _server.getPort();
}

unsigned int WebSocketServer::getTimeoutAsMilliseconds() const
{
    return _timeout.count();
}

std::chrono::milliseconds WebSocketServer::getTimeout() const
{
    return _timeout;
}

std::string WebSocketServer::getServeFolder() const
{
    return _serveFolder;
}

std::string WebSocketServer::getDefaultPage() const
{
    return _defaultPage;
}


WSInstantiator WebSocketServer::getInstantiator() const
{
    return _instantiator;
}

WSDestructor WebSocketServer::getDestructor() const
{
    return _destructor;
}

WSEventCallback WebSocketServer::getNewClientCallback() const
{
    return _onNewClient;
}

WSEventCallback WebSocketServer::getClosedClientCallback() const
{
    return _onClosedClient;
}

WSImasiCallback WebSocketServer::getUnknownMessageCallback() const
{
    return _onUnknownMessage;
}

const std::map<std::string, WSImasiCallback>& WebSocketServer::getMessageCallbacks() const
{
    return _messageCallbacks;
}