
import {is_uuid, json_parse, to_number} from "./src/string_functions.mjs";
import express from "express";
import bodyParser from "body-parser";
import {is_quarantine_hit} from "./src/spam_filter.mjs";
import {PartySearch} from "./src/PartySearch.class.mjs";
import {start_websocket_server} from "./src/websocket_server.mjs";
import {assert} from "./src/assert.mjs";
import * as http from "http";
import {get_ip_from_request, is_whitelisted} from "./src/auth.mjs";
import { WebSocket } from 'ws';
import morgan from "morgan";

const app = express();
app.use(bodyParser.text({type: '*/*'}));
/*app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
    extended: true
}));*/
app.use(morgan("tiny"));

/**
 *
 * @param arr {Array}
 * @param cb {Function}
 * @returns {Array}
 */
const unique = (arr,cb) => {
    let out = {};
    arr.forEach((item) => {
        const res = cb(item);
        if(out[res])
            return;
        out[res] = item;
    })
    return Object.values(out);
}


let parties_by_client = {};
let all_parties = [];
let last_party_change_timestamp_by_map = {};
let last_party_change = 0;
let last_available_maps_arr = [];
let bot_clients = {};

/**
 *
 * @param obj {Object}
 * @returns {boolean}
 */
function is_http(obj) {
    return obj instanceof http.IncomingMessage;
}
/**
 *
 * @param obj {Object}
 * @returns {boolean}
 */
function is_websocket(obj) {
    return obj instanceof WebSocket;
}

/**
 *
 * @param client_id {string}
 * @param request {http.IncomingMessage|WebSocket}
 */
function update_bot_client(client_id, request) {
    assert(is_uuid(client_id));
    bot_clients[client_id] = {
        ip: get_ip_from_request(request),
        last_message: (new Date()).getTime()
    }
}

function check_bot_clients() {
    const stale_ms = 1000 * 120; // 2 minute stale
    const stale_timestamp = (new Date()).getTime() - stale_ms;
    Object.keys(bot_clients).forEach((client_id) => {
        if(bot_clients[client_id].last_message < stale_timestamp) {
            on_bot_disconnected(client_id);
            delete bot_clients[client_id];
        }
    })
    setTimeout(check_bot_clients,5000);
}
check_bot_clients();

/**
 *
 * @param party {PartySearch}
 */
function remove_party(party) {
    const now = new Date();
    let idx = all_parties.indexOf(party);
    if(idx !== -1) {
        all_parties.splice(idx,1);
    }
    if(parties_by_client[party.client_id]) {
        idx = parties_by_client[party.client_id].indexOf(party);
        if(idx !== -1) {
            parties_by_client[party.client_id].splice(idx,1);
        }
    }

    last_party_change_timestamp_by_map[party.map_id] = now;
    last_party_change = now;
}
/**
 *
 * @param party {PartySearch}
 */
function add_party(party) {
    const now = new Date();
    if(!parties_by_client[party.client_id])
        parties_by_client[party.client_id] = [];
    parties_by_client[party.client_id].push(party);
    if(!all_parties.includes(party)) {
        all_parties.push(party);
    }
    last_party_change_timestamp_by_map[party.map_id] = now;
    last_party_change = now;
}

/**
 *
 * @param client_id {string}
 */
function on_bot_disconnected(client_id) {
    assert(is_uuid(client_id));
    console.log("on_bot_disconnected",client_id);
    delete bot_clients[client_id];
    let maps_affected = {};
    all_parties.filter((party) => { return party.client_id === client_id; })
        .forEach((party) => {
            maps_affected[party.map_id] = 1;
            remove_party(party);
        })
    Object.keys(maps_affected).forEach((map_id) => {
        send_map_parties(to_number(map_id));
    });
    send_available_maps();
}

/**
 * Bot has sent party information
 * @param ws {http.IncomingMessage|WebSocket}
 * @param data {Object}
 */
function on_recv_parties(ws, data) {
    assert(is_whitelisted(ws));
    assert(is_uuid(data.client_id));

    update_bot_client(data.client_id,ws);

    const map_id = to_number(data.map_id);
    const parties = data.parties.filter((party_json) => {
        return !is_quarantine_hit(party_json.message || '');
    }).map((party_json) => {
        party_json.client_id = data.client_id;
        party_json.district_region = data.district_region;
        party_json.map_id = map_id;
        return new PartySearch(party_json);
    });
    all_parties.filter((party) => { return party.client_id === data.client_id; })
        .forEach((party) => {
            remove_party(party);
        })
    parties.forEach((party) => {
        add_party(party);
    });

    send_map_parties(map_id);
    send_available_maps();
}
app.use('/', express.static('dist'));
app.use('/api', on_http_message);

/**
 *
 * @param request {Request|WebSocket}
 * @param obj {Object|string}
 */
function send_json(request,obj) {
    const json_str = typeof obj === 'string' ? obj : JSON.stringify(obj);
    if(is_websocket(request)) {
        request.send(json_str);
        return;
    }
    if(is_http(request)) {
        request.res.setHeader('Content-Type', 'application/json');
        request.res.end(json_str);
        return;
    }
    throw new Error("Invalid request object in send_json");
}

/**
 *
 * @param request {Request|WebSocket}
 * @param statusCode
 * @param statusMessage
 */
function send_header(request,statusCode,statusMessage = '') {
    if(is_websocket(request)) {
        request.send(JSON.stringify({
            "code":statusCode,
            "message":statusMessage
        }));
        return;
    }
    if(is_http(request)) {
        if(request.res.headersSent)
            return;
        request.res.status(statusCode).send(statusMessage);
        return;
    }
    throw new Error("Invalid request object in send_header");
}

/**
 *
 * @param ws {WebSocket|Request}
 * @param force
 */
function send_available_maps(ws = null, force = false) {
    const available_maps = unique(all_parties,(party) => {
        return party.map_id;
    }).map((party) => {
        return party.map_id;
    });
    last_available_maps_arr = available_maps;

    const res = JSON.stringify({
        "type":"available_maps",
        "map_ids":available_maps
    });

    if(ws === null) {
        Array.from(wss.clients).forEach((ws) => {
            ws.send(res);
        });
        return;
    }
    send_json(ws,res);
}

/**
 *
 * @param map_id {number}
 * @param request_or_websocket {WebSocket|Request|null}
 */
function send_map_parties(map_id, request_or_websocket = null) {
    const parties_by_map = all_parties.filter((party) => {
        return map_id === 0 || party.map_id === map_id;
    });
    const ret = JSON.stringify({
        "type":"map_parties",
        "map_id":map_id,
        "parties":parties_by_map
    });

    if(request_or_websocket === null) {
        Array.from(wss.clients).filter((ws) => {
            return ws.map_id === map_id;
        }).forEach((ws) => {
            ws.map_id = map_id;
            ws.send(ret);
        })
        return;
    }
    request_or_websocket.map_id = map_id;
    send_json(request_or_websocket,ret);
}

/**
 *
 * @param request_or_websocket {WebSocket|Request}
 */
function send_all_parties(request_or_websocket = null) {
    const ret = JSON.stringify({
        "type":"all_parties",
        "parties":all_parties
    });

    if(request_or_websocket === null) {
        Array.from(wss.clients).forEach((ws) => {
            ws.send(ret);
        })
        return;
    }
    send_json(request_or_websocket,ret);
}

/**
 *
 * @param request {Request}
 * @returns {Promise<Object>}
 */
async function get_data_from_request(request) {
    let data = null;
    switch(request.method.toLowerCase()) {
        case 'post':
            try {
                data = request.body;
                if(typeof data === 'string')
                    data = json_parse(data);
            } catch (e) {
                console.error(e);
            }
            break;
        case 'get':
            data = {};
            Object.keys(request.query).forEach((key) => {
                data[key] = request.query[key];
            });
            break;
    }
    return data;
}

/**
 *
 * @param request {Request|WebSocket}
 * @param data {Object}
 * @returns {Promise<void>}
 */
async function on_request_message(request, data) {
    switch(data.type || '') {
        case "map_id":
            send_map_parties(to_number(data.map_id), request);
            break;
        case "client_disconnect":
            if(is_uuid(data.client_id) && bot_clients[data.client_id] && bot_clients[data.client_id].ip === get_ip_from_request(request)) {
                on_bot_disconnected(data.client_id);
            }
            break;
        case "client_info":
            if(is_uuid(data.client_id) && is_whitelisted(request)) {
                if(is_websocket(request))
                    request.client_id = data.client_id;
                update_bot_client(data.client_id,request);
            }
            break;
        case "client_parties":
            on_recv_parties(request, data);
            break;
        case "available_maps":
            send_available_maps(request);
            break;
        case "map_parties":
            send_map_parties(to_number(data.map_id),request);
            break;
        case "all_parties":
            send_all_parties(request);
            break;
        default:
            send_header(request, 400,"Invalid or missing type parameter");
            break;
    }
}

/**
 *
 * @param request {Request}
 * @param response {Response}
 */
async function on_http_message(request,response) {
    console.log(`Http message ${request.method} ${request.url}`);
    try {
        let data = await get_data_from_request(request);
        if(!data) {
            response.sendStatus(500);
            return;
        }
        console.log("Http data type %s",data.type || 'unknown');
        await on_request_message(request, data);
        send_header(request, 200);
    } catch(e) {
        console.error(e);
        send_header(request, 500,e.message);
    }
}

/**
 *
 * @param data {string}
 * @param ws {WebSocket}
 * @returns {Promise<void>}
 */
async function on_websocket_message(data, ws) {
    console.log('Websocket message from %s',get_ip_from_request(ws));
    try {
        data = json_parse(data);
        console.log("Websocket data type %s",data.type || 'unknown');
        await on_request_message(ws, data);
    } catch(e) {
        console.error(e);
    }
}

const http_server = http.createServer();
http_server.on('request', app);
http_server.listen(80, function() {
    console.log("http/ws server listening on port 80");
});

const wss = start_websocket_server(http_server);
wss.on('connection', function connection(ws) {
    ws.originalSend = ws.send;
    ws.send = (data) => {
        ws.originalSend(data);
    }
    ws.map_id = 0;
    if(all_parties.length) {
        // TODO: Default the client to the most active map atm
        send_map_parties(all_parties[0].map_id, ws);
    }
    send_available_maps(ws, true);

    ws.on('message',(data) => {
        on_websocket_message(data,ws).catch((e) => {
            console.error(e);
        })
    });
    ws.on('pong', () => {
        if(ws.client_id) {
            assert(is_uuid(ws.client_id));
            update_bot_client(ws.client_id,ws);
        }
    });
    ws.on('close',() => {
        if(ws.client_id) {
            on_bot_disconnected(ws.client_id);
        }
    })
});
