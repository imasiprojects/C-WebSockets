"use strict";

// Main Static Object (as namespace)
var ImasiSoftware = ImasiSoftware || {};

ImasiSoftware.WebSocketClient = function() {

    this.webSocket = null;
    this.eventList = new Array();

    this.onConnect = function() {
        alert("Connected");
    };
    this.onClose = function() {
        alert("Connection closed");
    };
    this.onError = function() {
        alert("Error");
    };
    this.onMessage = function(key, value) {};

};

ImasiSoftware.WebSocketClient.prototype.connect = function (host, port) {

    var that = this;

    if ("WebSocket" in window) {

        this.webSocket = new WebSocket("ws://" + host + ":" + port + "/");
        this.webSocket.onopen = this.onConnect;
        this.webSocket.onclose = this.onClose;
        this.webSocket.onerror = this.onError;
        this.webSocket.onmessage = function (ev) {
            var keySize = ev.data.charCodeAt(0) + 1;

            var key = ev.data.substring(1, keySize);
            var value = ev.data.substring(keySize);

            var event = that.eventList[key] || null;
            if (event !== null) {
                event(value);
            } else {
                that.onMessage(key, value);
            }
        };

    } else {

        alert("WebSocket is not supported on your Browser. UPDATE TO MOZILLA NOOOB!!");

    }

}

ImasiSoftware.WebSocketClient.prototype.send = function(key, value) {

    this.webSocket.send(String.fromCharCode(key.length) + key + value);

}

ImasiSoftware.WebSocketClient.prototype.addEventListener = function(key, callback) {

    this.eventList[key] = callback;

}

ImasiSoftware.WebSocketClient.prototype.removeEventListener = function(key) {

    this.eventList[key] = null;

}