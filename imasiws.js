"use strict";

// Main Static Object (as namespace)
var ImasiSoftware = ImasiSoftware || {};

ImasiSoftware.WebSocketClient = function() {

    var that = this;

    // private
    this.webSocket = null;

    // public
    this.onConnect = function() {
        alert("Connected");
        that.send("Key", "Meessssage");
    };
    this.onClose = function() {
        alert("Connection closed");
    };
    this.onError = function() {
        alert("Error");
    };
    this.onMessage = function(key, value) {
        alert("Message received: {" + key + "} " + value);
    };

};

ImasiSoftware.WebSocketClient.prototype.connect = function (host, port) {

    var that = this;

    if ("WebSocket" in window) {

        this.webSocket = new WebSocket("ws://" + host + ":" + port + "/");
        this.webSocket.onopen = this.onConnect;
        this.webSocket.onclose = this.onClose;
        this.webSocket.onerror = this.onError;
        this.webSocket.onmessage = function (ev) {

            alert("FULL DATA: " + ev.data);

            var keySize = ev.data.charCodeAt(0) + 1;
            that.onMessage(ev.data.substring(1, keySize), ev.data.substring(keySize));
        };

    } else {

        alert("WebSocket is not supported on your Browser. UPDATE TO MOZILLA NOOOB!!");

    }

}

ImasiSoftware.WebSocketClient.prototype.send = function(key, value) {

    this.webSocket.send(String.fromCharCode(key.length) + key + value);

}