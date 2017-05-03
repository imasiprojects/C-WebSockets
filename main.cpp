#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <sstream>

#include "Sockets.hpp"
#include "NewWebSocketServer.hpp"


/*class CustomConnection : public WebSocketConnection
{
    static int nextId;
    int id;

public:

    CustomConnection (WebSocketServer* server, Connection conn)
        : WebSocketConnection (server, conn), id (0)
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


void startServer()
{
    WebSocketServer server;

    server.setNewClientCallback([](WebSocketServer* server, WebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        conn->setId();
        std::cout << "New client entered with IP: " << connection->getIp() << ", ID: " << conn->getId() << std::endl;
        connection->send("saludo", "hola");
    });

    server.setClosedClientCallback([](WebSocketServer* server, WebSocketConnection* connection)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Client with ID: " << conn->getId() << " disconnected." << std::endl;
    });

    server.setUnknownMessageCallback([](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "ID: " << conn->getId() << " -> Unknown: [" << key << "] = " << data << std::endl;
        connection->send("his", "\x01\x02\x03\x04");
    });

    server.setDataCallback("Prueba", [](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
    {
        CustomConnection* conn = (CustomConnection*) connection;
        std::cout << "Id: " << conn->getId() << " -> [" << key << "] = " << data << std::endl;
        server->sendPing();
    });

    server.setInstantiator([](WebSocketServer* server, Connection conn)->WebSocketConnection*
    {
        return new CustomConnection(server, conn);
    });

    server.setServeFolder("../c-websockets");
    server.setDefaultPage("client.html");

    if (!server.startAndWait(80))
    {
        std::cout << "Server couldn't be started" << std::endl;
    }
}*/

int main (int argc, char** argv)
{
    /*startServer ();

    std::cout << "Finished" << std::endl;
    std::cin.get ();*/

    NewWebSocketServer server;
    server.start(80, 10);

    std::this_thread::sleep_for(std::chrono::minutes(10));

    return 0;
}