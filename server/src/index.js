import {start_websocket_client} from "./websocket_client.mjs";
import {district_regions, getMapName, languages, party_search_types} from "./gw_constants.mjs";
import {to_number} from "./string_functions.mjs";

window.all_parties = [];
window.available_maps = [];
window.current_map = 0;

let ws = null;

const party_table = document.querySelector('table tbody');
const party_header = document.querySelector('h1');
const map_filter = document.querySelector("#map-filter");
const language_filter = document.querySelector("#language-filter");
const search_type_filter = document.querySelector("#search-type-filter");
const district_filter = document.querySelector("#district-filter");

map_filter.addEventListener('change',function() {
    console.log(map_filter.value);
    window.current_map = to_number(map_filter.value);
    if(ws) {
        ws.send(JSON.stringify({
            "type":"map_id",
            "map_id":window.current_map
        }))
    }
})
language_filter.addEventListener('change',function() {
    redrawPartySearches();
})
search_type_filter.addEventListener('change',function() {
    redrawPartySearches();
})
district_filter.addEventListener('change',function() {
    redrawPartySearches();
})

Object.keys(district_regions).forEach((district_region_id) => {
    const option = document.createElement('option');
    option.value = district_region_id;
    option.innerHTML = district_regions[district_region_id].name;
    district_filter.appendChild(option);
})
languages.forEach((language, language_id) => {
    if(!language)
        return;
    const option = document.createElement('option');
    option.value = language_id.toString();
    option.innerHTML = language.name;
    language_filter.appendChild(option);
})
party_search_types.forEach((name, id) => {
    const option = document.createElement('option');
    option.value = id.toString();
    option.innerHTML = name;
    search_type_filter.appendChild(option);
})

function getFilteredPartySearches() {
    const district_region = district_filter.value === '' ? false : to_number(district_filter.value);
    const district_language = language_filter.value === '' ? false : to_number(language_filter.value);
    const search_type = search_type_filter.value === '' ? false : to_number(search_type_filter.value);
    return window.all_parties.filter((party) => {
        return party.map_id === window.current_map
            && (search_type === false || party.search_type === search_type)
            && (district_language === false || party.district_language === district_language)
            && (district_region === false || party.district_region === district_region);
    })
}

function redrawAvailableMaps() {
    let maps_filter_options = map_filter.querySelector('option[value="0"]').outerHTML;
    let selected = window.current_map;
    console.log("redrawAvailableMaps",map_filter, selected,window.available_maps);
    window.available_maps.forEach((map_id) => {
        const option = document.createElement('option');
        option.value = map_id.toString();
        if(map_id === selected) {
            option.selected = true;
        }
        option.innerHTML = getMapName(map_id);
        maps_filter_options += option.outerHTML;
    })
    map_filter.innerHTML = maps_filter_options;
    map_filter.value = selected;
}
function redrawPartySearches() {
    console.log("redrawPartySearches");

    redrawAvailableMaps();

    party_header.innerHTML = `Viewing parties for ${getMapName(window.current_map)}`;

    let party_table_rows = '';
    const parties = getFilteredPartySearches();
    parties.forEach((party) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td>${party.search_type_name}</td>
                <td>${party.sender || ''} ${party.hard_mode ? '[Hard Mode]' :''}</td>
                <td title="${party.party_size} players, ${party.hero_count} heros" class="text-center">${party.party_size}/${party.hero_count || '0'}</td>
                <td title="${party.district_name}" class="text-center">${party.district_abbr}</td>
                <td>${party.message || ''}</td>`;
        party_table_rows += tr.outerHTML;
    });
    party_table.innerHTML = party_table_rows;
}

function reconnect_websocket_client() {
    console.log("reconnect_websocket_client");
    ws = start_websocket_client();
    ws.addEventListener("close",(event) => {
        setTimeout(reconnect_websocket_client,5000);
    })
    ws.addEventListener("message",(event) => {
        redrawPartySearches();
    })
}
reconnect_websocket_client();


