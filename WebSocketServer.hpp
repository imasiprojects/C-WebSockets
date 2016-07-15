#pragma once



class WebSocketServer {
    /// Native
    Callback onNewClient;
    Callback onUnknownClientMessage;

    /// IMASI Protocol
    map<string, IMASICallback> clientMsg;


    list<thread> clientThreads;
    static void clientThreadFunction(WebSocketServer& srv);

public:
    WebSocketServer(); // Do nothing
    ~WebSocketServer(); // Wait threads

    start();
    close();

    /// Native
    setClientCallback(Callback callback)
    setNewClientCallback(Callback callback)

    send(string data)
    sendBroadcast(string data)


    /// IMASI Protocol
    addClientCallback(string key, IMASICallback callback)

    send(string key, string data)
    sendBroadcast(string key, string data)

};
