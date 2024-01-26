import { WebSocketServer } from 'ws';
import {assert} from './assert.mjs'

let connected_sockets = [];

function check_connections(wss) {
    if(wss) {
        wss.clients.forEach((ws) => {
            if (ws.isAlive === false) {
                console.log("Terminating timed out ws");
                ws.terminate();
                return;
            }
            ws.isAlive = false;
            ws.ping();
        });
    }
    setTimeout(() => {
        check_connections(wss);
    },30000);
}

function websocket_send(payload) {
    this.last_ping = new Date();
    return this.original_send(payload);
}


export function start_websocket_server(http_server) {
    let wss = 0;
    assert(!wss);
    if(typeof http_server == 'number') {
        wss = new WebSocketServer({port:http_server});
        console.log("Webserver started, port %d",http_server);
    } else {
        wss = new WebSocketServer({server:http_server});
    }


    check_connections(wss);

    wss.on('close', function close() {
        wss = null;
    });
    wss.on('connection', function connection(ws) {
        console.log(`Websocket client connected`);
        ws.isAlive = true;
        ws.on('error', console.error);
        ws.on('pong', () => {
            ws.isAlive = true;
        });
        ws.on('message', function message(data) {
            ws.isAlive = true;
            console.log('received: %s', data);
        });
        ws.on('close',() => {
            console.log(`Websocket client closed`);
        });
    });
    return wss;
}

