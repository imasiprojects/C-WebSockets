#include "WebSocketServer.hpp"

WebSocketServer::WebSocketServer(){
    _onNewClient = nullptr;
    _onUnknownClientMessage = nullptr;
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
    #error TODO: Join threads
}

bool WebSocketServer::newClient(){
    if(!_server.isOn())
        return false;
    Connection conn = _server.newClient();
    if(conn.sock == SOCKET_ERROR)
        return false;

    _clientThreads.push_back(
        new std::thread(clientThreadFunction,
                   std::ref(*this),
                   conn)
    );
    return true;
}
