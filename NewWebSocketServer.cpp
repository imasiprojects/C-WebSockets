#include "NewWebSocketServer.hpp"



NewWebSocketServer::NewWebSocketServer()
    : _threadPool(nullptr)
    , _isStopping(false)
    , _acceptNewClients(true)
{
}


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

        _threadPool = new ThreadPool(eventHandlerThreadCount + 3);


        _threadPool->addTask([]() {});
        // TODO: Add NewClients and HTTPWebSocketProtocol tasks
    }

    return false;
}

void NewWebSocketServer::stop()
{
    if (isRunning())
    {
        _isStopping = true;

        delete _threadPool;
        _server.finish();

        _isStopping = false;
    }
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