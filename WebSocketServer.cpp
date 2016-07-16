#include "WebSocketServer.hpp"
#include "HttpHelper.hpp"
#include "Sha1.hpp"
#include "Base64.hpp"
#include "WebSocket.hpp"

WebSocketServer::WebSocketServer()
:_acceptNewClients(true),_onNewClient(nullptr),_onUnknownMessage(nullptr){
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

bool WebSocketServer::startAndWait(unsigned short port){
    if(_server.isOn())
        close();
    bool ok = _server.start(port);
    if(!ok)
        return false;

    _server.setBlocking(false);

    while (isRunning()){
        if(_acceptNewClients)
            acceptNewClient();
        clearClosedConnections();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return true;
}

void WebSocketServer::close(){
    for(WebSocketConnection* wsc:_connections){
        wsc->stopAndWait();
        delete wsc;
    }
    _connections.clear();
    _server.finish();
}

unsigned short WebSocketServer::getPort() const{
    return _server.getPort();
}

void WebSocketServer::setAcceptNewClients(bool acceptNewClients){
    _acceptNewClients = acceptNewClients;
}

bool WebSocketServer::isAcceptingNewClients() const{
    return _acceptNewClients;
}

bool WebSocketServer::isRunning() const{
    return _server.isOn();
}

bool WebSocketServer::acceptNewClient(){
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

bool WebSocketServer::clearClosedConnections(){
    for(auto it=_connections.begin(); it!=_connections.end();){
        if(!(*it)->isRunning()){
            delete *it;
            auto temp = it++;
            _connections.erase(temp);
        }else ++it;
    }
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

bool WebSocketServer::setUnknownMessageCallback(WSImasiCallback callback){
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

void WebSocketServer::sendPing()
{
	for (WebSocketConnection* conn : _connections)
	{
		conn->ping();
	}
}

WSNewClientCallback WebSocketServer::getNewClientCallback() const{
    return _onNewClient;
}

WSImasiCallback WebSocketServer::getUnknownMessageCallback() const{
    return _onUnknownMessage;
}

const std::map<std::string, WSImasiCallback>& WebSocketServer::getMessageCallbacks() const {
    return _messageCallbacks;
}

WebSocketConnection::WebSocketConnection(WebSocketServer* server, Connection conn)
:_thread(nullptr),_server(server),_handShakeDone(false),_isRunning(true),_mustStop(false),_lastPingRequestTime(clock()){
    _conn.connect(conn.sock, conn.ip, _server->getPort());
    _thread = new std::thread(&WebSocketConnection::threadFunction, this);
}

WebSocketConnection::~WebSocketConnection(){
    stopAndWait();
    delete _thread;
}

std::string WebSocketConnection::getIp() const{
    return _conn.getIp();
}

void WebSocketConnection::send(std::string key, std::string data){

	this->_conn.send(WebSocket::mask((char)key.size() + key + data));

}

void WebSocketConnection::stop(){
    _mustStop = true;
}

void WebSocketConnection::stopAndWait(){
    stop();
    if(_thread!=nullptr && _thread->joinable())
        _thread->join();
}

bool WebSocketConnection::isRunning() const{
    return _isRunning;
}

bool sleep(unsigned int ms = 1)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	return false;
}

void WebSocketConnection::threadFunction()
{
	// Handshake + Loop (leer mensajes)
	while (!this->_mustStop && this->_conn.isConnected())
	{
		std::string buffer;
		std::string finBuffer;
		char finOpCode;
		if (_handShakeDone)
		{
			do
			{
				if (_mustStop)
				{
					this->_isRunning = false;
					return;
				}
				buffer += this->_conn.recv(2 - buffer.size());
			}
			while (buffer.size() < 2 || sleep());

			bool fin = buffer[0] & 0x80;
			char opCode = buffer[0] & 0xF;
			bool hasMask = buffer[1] & 0x80;
			long packetSize = buffer[1] & 127;
			if (packetSize == 126)
			{
				do
				{
					if (_mustStop)
					{
						this->_isRunning = false;
						return;
					}
					buffer += this->_conn.recv(4 - buffer.size());
				}
				while (buffer.size() < 4 || sleep());
				packetSize = *(short*)&buffer[2];
			}
			else if (packetSize == 127)
			{
				do
				{
					if (_mustStop)
					{
						this->_isRunning = false;
						return;
					}
					buffer += this->_conn.recv(10 - buffer.size());
				}
				while (buffer.size() < 10 || sleep());
				packetSize = *(long long*)&buffer[2];
			}

			std::string mask;
			if (hasMask)
			{
				do
				{
					if (_mustStop)
					{
						this->_isRunning = false;
						return;
					}
					mask += this->_conn.recv(4 - mask.size());
				}
				while (mask.size() < 4 || sleep());
			}

			buffer.clear();
			do
			{
				if (_mustStop)
				{
					this->_isRunning = false;
					return;
				}
				buffer += this->_conn.recv(packetSize - buffer.size());
			}
			while (buffer.size() < packetSize || sleep());

			std::string data = hasMask ? WebSocket::unmask(mask, buffer) : buffer;

			if (fin)
			{
				if (opCode == 0x0)
				{
					data = finBuffer + data;
					finBuffer.clear();
				}

				switch (opCode)
				{
					case 0x8:
					{
						// Connection close
						this->_conn.disconnect();
						break;
					}
					case 0x9:
					{
						// Ping
						pong(data);
						break;
					}
					case 0xA:
					{
						// Pong
						/// TODO: comprobar si es igual al ultimo PING, si lo es hay que reiniciar el contador y ya.
						std::cout << "PONG" << std::endl;
						break;
					}
					default:
					{
						std::string key = data.substr(1, data[0]);
						std::string value = data.substr(data[0] + 1);

						std::map<std::string, WSImasiCallback> dataCallBacks = this->_server->getMessageCallbacks();
						if (dataCallBacks.find(key) != dataCallBacks.end())
						{
							dataCallBacks[key](this->_server, this, key, value);
						}
						else
						{
							WSImasiCallback uknownDataCallback = this->_server->getUnknownMessageCallback();
							if (uknownDataCallback != nullptr) uknownDataCallback(this->_server, this, key, value);
						}
						break;
					}
				}
			}
			else
			{
				if (opCode != 0x0)
				{
					finOpCode = opCode;
				}
				finBuffer += data;
			}
		}
		else
		{
			do
			{
				if (_mustStop)
				{
					this->_isRunning = false;
					return;
				}
				buffer += this->_conn.recv();
			}
			while (buffer.rfind("\r\n\r\n") == std::string::npos || sleep());
			_handShakeDone = performHandShake(buffer);
			if(_handShakeDone){
                WSNewClientCallback temp = _server->getNewClientCallback();
                if(temp != nullptr)
                    temp(_server, this);
			}
		}
	}

	this->_isRunning = false;
}

bool WebSocketConnection::performHandShake(std::string buffer)
{
	std::string websocketKey = HttpHelper::getHeaderValue(buffer, "Sec-WebSocket-Key");
	if (websocketKey.size() > 0)
	{
		std::string sha1Key = Sha1::sha1UnsignedChar(websocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		std::string finalKey = Base64::encode(sha1Key.c_str());

		this->_conn.send("HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: chat\r\nSec-WebSocket-Accept: " + finalKey + "\r\n\r\n");
		return true;
	}
	return false;
}

void WebSocketConnection::ping()
{
	_lastPingRequest = "Imasi Projects Te Pingea"; // TODO: algo random here
	this->_conn.send(WebSocket::mask(_lastPingRequest, 0x9));
}

void WebSocketConnection::pong(std::string data)
{
	if (data == _lastPingRequest)
	{
		_lastPingRequestTime = clock();
	}
}
