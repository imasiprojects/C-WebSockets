#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <sstream>

#include "Sockets.hpp"
#include "NewWebSocketServer.hpp"


class CustomConnection : public NewWebSocketConnection
{
    static int nextId;
    int id;

public:

    CustomConnection (NewWebSocketServer* server, ClientData* clientData)
        : NewWebSocketConnection(server, clientData), id (0)
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


NewWebSocketServer* startServer()
{
    NewWebSocketServer* server = new NewWebSocketServer();

    server->setNewClientCallback([](NewWebSocketServer* server, NewWebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        conn->setId();
        std::cout << "New client entered with IP: " << connection->getIp() << ", ID: " << conn->getId() << std::endl;
        connection->send("saludo", "hola");
    });

    server->setClosedClientCallback([](NewWebSocketServer* server, NewWebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Client with ID: " << conn->getId() << " disconnected." << std::endl;
    });

    server->setUnknownMessageCallback([](NewWebSocketServer* server, NewWebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "ID: " << conn->getId() << " -> Unknown: [" << key << "] = " << data << std::endl;
        connection->send("his", "\x01\x02\x03\x04");
    });

    server->setDataCallback("Prueba", [](NewWebSocketServer* server, NewWebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Id: " << conn->getId() << " -> [" << key << "] = " << data << std::endl;
        server->pingAll("Ping a todos");
    });

    server->setInstantiator([](NewWebSocketServer* server, ClientData* clientData) -> NewWebSocketConnection*
    {
        return new CustomConnection(server, clientData);
    });

    server->setServeFolder("../c-websockets");
    server->setDefaultPage("client.html");

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
    NewWebSocketServer* server = startServer();

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