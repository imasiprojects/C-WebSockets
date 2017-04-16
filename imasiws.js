"use strict";

var ImasiSoftware = ImasiSoftware || {};

ImasiSoftware.WebSocketClient = function()
{

    this.webSocket = null;
    this.eventList = new Array();

    this.onConnect = function()
    {
        alert("Connected");
    };

    this.onClose = function ()
    {
        alert("Connection closed");
    };

    this.onError = function ()
    {
        alert("Error");
    };

    this.onMessage = function(key, value) {};
};

ImasiSoftware.WebSocketClient.prototype.connect = function (host, port) {
    var _this = this;

    if ("WebSocket" in window) {
        this.webSocket = new WebSocket("ws://" + host + ":" + port + "/");
        this.webSocket.binaryType = "arraybuffer";
        this.webSocket.onopen = this.onConnect;
        this.webSocket.onclose = this.onClose;
        this.webSocket.onerror = this.onError;

        this.webSocket.onmessage = function (ev) {
            var key = null;
            var value = null;

            if (ev.data instanceof ArrayBuffer)
            {
                var uint8Array = new Uint8Array(ev.data);
                var keySize = uint8Array[0];

                key = uint8Array.slice(1, keySize + 1);
                value = uint8Array.slice(keySize + 1);

                var decoder = new TextDecoder("utf-8");

                key = decoder.decode(key);
                value = decoder.decode(value);
            }
            else
            {
                var keySize = ev.data.charCodeAt(0);

                key = ev.data.substring(1, keySize + 1);
                value = ev.data.substring(keySize + 1);
            }

            var event = _this.eventList[key] || _this.onMessage;

            event(key, value);
        };
    } else {
        alert("WebSocket is not supported on your Browser");
    }
}

ImasiSoftware.WebSocketClient.prototype.send = function (key, value)
{
    this.webSocket.send(String.fromCharCode(key.length) + key + value);
}

ImasiSoftware.WebSocketClient.prototype.addEventListener = function (key, callback)
{
    this.eventList[key] = callback;
}

ImasiSoftware.WebSocketClient.prototype.removeEventListener = function (key)
{
    this.eventList[key] = null;
}