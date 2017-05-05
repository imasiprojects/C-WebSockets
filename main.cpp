#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <sstream>

#include "Sockets.hpp"
#include "WebSocketServer.hpp"


class CustomConnection : public WebSocketConnection
{
    static int nextId;
    int id;

public:

    CustomConnection (WebSocketServer* server, ClientData* clientData)
        : WebSocketConnection(server, clientData), id (0)
    {
    }

    int getId () const
    {
        return id;
    }

    void setId ()
    {
        if (id <= 0)
        {
            id = nextId++;
        }
    }

};

int CustomConnection::nextId = 1;


WebSocketServer* startServer()
{
    WebSocketServer* server = new WebSocketServer();

    server->setNewClientCallback([](WebSocketServer* server, WebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        conn->setId();
        std::cout << "New client entered with IP: " << connection->getIp() << ", ID: " << conn->getId() << std::endl;
        connection->send("saludo", "hola");
    });

    server->setClosedClientCallback([](WebSocketServer* server, WebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Client with ID: " << conn->getId() << " disconnected." << std::endl;
    });

    server->setUnknownMessageCallback([](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "ID: " << conn->getId() << " -> Unknown: [" << key << "] = " << data << std::endl;
        connection->send("his", "\x01\x02\x03\x04");
    });

    server->setDataCallback("Prueba", [](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Id: " << conn->getId() << " -> [" << key << "] = " << data << std::endl;
        server->pingAll("Ping a todos");
    });

    server->setInstantiator([](WebSocketServer* server, ClientData* clientData) -> WebSocketConnection*
    {
        return new CustomConnection(server, clientData);
    });

    server->setServeFolder("/");
    server->setDefaultPage("client.html");

    server->setTimeout(5000);

    if (server->start(80, 10))
    {
        std::cout << "Server started" << std::endl;
    }
    else
    {
        std::cout << "Server couldn't be started" << std::endl;

        delete server;
        server = nullptr;
    }

    return server;
}

int main (int argc, char** argv)
{
    WebSocketServer* server = startServer();

    if (server != nullptr)
    {
        while (server->isRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        delete server;
    }

    return 0;
}