#include "NewWebSocketServer.hpp"



NewWebSocketServer::NewWebSocketServer(size_t threadCount)
    : _threadPool(threadCount)
{
}


NewWebSocketServer::~NewWebSocketServer()
{
}

// Setters

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
    return _server.isOn();
}

bool NewWebSocketServer::isAcceptingNewClients() const
{
    return _acceptNewClients;
}

unsigned short NewWebSocketServer::getPort() const
{
    return _server.getPort();
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