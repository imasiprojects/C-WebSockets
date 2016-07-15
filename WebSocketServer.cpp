#include "WebSocketServer.hpp"

WebSocketServer::WebSocketServer()
:_onNewClient(nullptr),_onUnknownMessage(nullptr){
    _onNewClient = nullptr;
    _onUnknownMessage = nullptr;
}

WebSocketServer::~WebSocketServer(){
    close();
}

bool WebSocketServer::start(unsigned short port){
    if(_server.isOn())
        close();
    bool ok = _server.start(port);
    if(ok)
        _server.setBlocking(true);
    return ok;
}

void WebSocketServer::close(){
    for(WebSocketConnection* wsc:_connections){
        wsc->stopAndWait();
        delete wsc;
    }
    _connections.clear();
    _server.finish();
}

bool WebSocketServer::isRunning() const{
    return _server.isOn();
}

bool WebSocketServer::newClient(){
    if(!_server.isOn())
        return false;
    Connection conn = _server.newClient();
    if(conn.sock == SOCKET_ERROR)
        return false;

    _connections.insert(new WebSocketConnection(
        this, conn
    ));
    return true;
}

bool WebSocketServer::setNewClientCallback(WSNewClientCallback callback){
    if(isRunning())
        return false;
    _onNewClient = callback;
    return true;
}

bool WebSocketServer::setDataCallback(std::string key, WSImasiCallback callback){
    if(isRunning())
        return false;
    _messageCallbacks[key] = callback;
    return true;
}

bool WebSocketServer::setUnknownDataCallback(WSImasiCallback callback){
    if(isRunning())
        return false;
    _onUnknownMessage = callback;
    return true;
}

void WebSocketServer::sendBroadcast(std::string key, std::string data){
    for(WebSocketConnection* conn:_connections){
        conn->send(key, data);
    }
}

WebSocketServer::WebSocketConnection::WebSocketConnection(WebSocketServer* server, Connection conn)
:_thread(nullptr),_server(server),_isRunning(false),_mustStop(false){
    _conn.connect(conn.sock, conn.ip, this->_server->_server.getPort());
    _thread = new std::thread(&threadFunction, this);
}

WebSocketServer::WebSocketConnection::~WebSocketConnection(){
    stopAndWait();
}

void WebSocketServer::WebSocketConnection::send(std::string key, std::string data){
    #error TODO
}

void WebSocketServer::WebSocketConnection::stop(){
    _mustStop = true;
}

void WebSocketServer::WebSocketConnection::stopAndWait(){
    stop();
    if(_isRunning && _thread!=nullptr && _thread->joinable())
        _thread->join();
}

bool WebSocketServer::WebSocketConnection::isRunning() const{
    return _isRunning;
}
