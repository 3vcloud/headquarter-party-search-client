
// Create WebSocket connection.
import {assert} from './assert.mjs'
import {PartySearch} from "./PartySearch.class.mjs";

let ws = null;

export function start_websocket_client() {
    assert(!ws);
    const url = new URL(window.location.href);
    let websocket_url='';
    if(url.protocol === 'https:') {
        websocket_url = `wss://${url.host}`;
    } else {
        websocket_url = `ws://${url.host}`;
    }
    ws = new WebSocket(websocket_url);

// Connection opened
    ws.addEventListener("open", (event) => {
        console.log("Websocket connected successfully",event);
    });

    ws.addEventListener("close",(event) => {
        console.log("Websocket close",event);
        ws = null;
    })

// Listen for messages
    ws.addEventListener("message", (event) => {
        console.log("Websocket message",event);
        let json = null;
        try {
            json = JSON.parse(event.data);
        } catch(e) {
            console.error("Failed to decode JSON");
            return;
        }
        console.log("Websocket JSON",json);
        switch(json.type || '') {
            case "all_parties":
                assert(Array.isArray(json.parties));
                window.current_map = 0;
                window.all_parties = json.parties.map((party_json) => {
                    return new PartySearch(party_json);
                });
                break;
            case "map_parties":
                assert(typeof json.map_id === 'number');
                assert(Array.isArray(json.parties));
                window.current_map = json.map_id;
                window.all_parties = json.parties.map((party_json) => {
                    return new PartySearch(party_json);
                });
                break;
            case "available_maps":
                assert(Array.isArray(json.map_ids));
                window.available_maps = json.map_ids;
                break;
        }
    });
    return ws;
}