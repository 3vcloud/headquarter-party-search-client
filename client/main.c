#ifndef __STDC__
#define __STDC__ 1
#endif

#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <locale.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <jansson.h>

#define HEADQUARTER_RUNTIME_LINKING
#include <client/constants.h>
#include <client/Headquarter.h>
#include <common/time.h>
#include <common/thread.h>
#include <common/dlfunc.h>

#include "gw-helper.c"
#ifdef _WIN32
# define DllExport __declspec(dllexport)
#else
# define DllExport
#endif

#include <signal.h>
#include <stdio.h>


typedef struct {
    uint16_t            party_id;
    uint8_t             party_size;
    uint8_t             hero_count;
    uint8_t             search_type; // 0=hunting, 1=mission, 2=quest, 3=trade, 4=guild
    uint8_t             hardmode;
    uint16_t            district_number;
    uint8_t             language;
    uint8_t             primary;
    uint8_t             secondary;
    uint8_t             level;
    char                message[65];
    char                sender[41];
} PartySearchAdvertisement;

#define MapID_EmbarkBeach               857
#define MapID_Ascalon                   148
#define MapID_Kamadan                   449
#define MapID_Kamadan_Halloween         818
#define MapID_Kamadan_Wintersday        819
#define MapID_Kamadan_Canthan_New_Year  820

static volatile bool  running;
static struct thread  bot_thread;
static PluginObject* bot_module;
static char* server_domain = "http://localhost";
static char server_url[128];
static char websocket_url[128];
static volatile bool pending_send_party_search_advertisements = false;

static int required_map_id = MapID_Kamadan;
static District required_district = DISTRICT_AMERICAN;
static uint16_t required_district_number = 0;

static CallbackEntry EventType_PartySearchAdvertisement_entry;
static CallbackEntry EventType_PartySearchRemoved_entry;
static CallbackEntry EventType_PartySearchSize_entry;
static CallbackEntry EventType_PartySearchType_entry;
static CallbackEntry EventType_WorldMapEnter_entry;
static CallbackEntry EventType_WorldMapLeave_entry;

static uint16_t player_name[20];
static char player_name_utf8[40];
static char account_uuid[64];

static uint64_t last_successful_curl = 0;

static void send_bot_info();

static array(PartySearchAdvertisement) party_search_advertisements;

static CURL* init_curl(const char* url) {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);
    static struct curl_slist* slist = NULL;
    if (!slist) {
        slist = curl_slist_append(slist, "Accept:");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    return curl;
}

static json_t* party_search_advertisement_to_json(PartySearchAdvertisement* party) {
    json_t* party_json = json_object();
    json_object_set_new( party_json, "party_id", json_integer( party->party_id ) );
    json_object_set_new( party_json, "district_number", json_integer( party->district_number ) );
    json_object_set_new( party_json, "district_language", json_integer( party->language ));
    json_object_set_new( party_json, "party_size", json_integer( party->party_size ));
    json_object_set_new( party_json, "hero_count", json_integer( party->hero_count ));
    json_object_set_new( party_json, "search_type", json_integer( party->search_type ));
    json_object_set_new( party_json, "hard_mode", json_integer( party->hardmode ));
    json_object_set_new( party_json, "primary", json_integer( party->primary ));
    json_object_set_new( party_json, "secondary", json_integer( party->secondary ));
    json_object_set_new( party_json, "level", json_integer( party->level ));
    if(party->message[0])
        json_object_set_new( party_json, "message", json_string(party->message) );
    if(party->sender[0])
        json_object_set_new( party_json, "sender", json_string(party->sender) );
    return party_json;
}

static void remove_all_party_search_advertisements(bool free_memory) {
    PartySearchAdvertisement* party = NULL;

    array_foreach(party, &party_search_advertisements) {
        party->party_id = 0;
    }
    if (free_memory) 
        array_reset(&party_search_advertisements);
}

static PartySearchAdvertisement* get_party_search_advertisement(uint32_t party_search_id) {
    PartySearchAdvertisement* party;
    array_foreach(party, &party_search_advertisements) {
        if (party->party_id == party_search_id) {
            return party;
        }
    }
    return NULL;
}

static PartySearchAdvertisement* create_party_search_advertisement(Event* event) {
    assert(event && event->PartySearchAdvertisement.party_id);

    PartySearchAdvertisement* party = get_party_search_advertisement(event->PartySearchAdvertisement.party_id);
    if (!party) {
        party = get_party_search_advertisement(0); // Get first empty slot.
        if (!party) {
            array_resize(&party_search_advertisements, party_search_advertisements.size + 1);
            party = get_party_search_advertisement(0);
        }
    }
    assert(party);

    party->party_id = event->PartySearchAdvertisement.party_id;
    party->party_size = event->PartySearchAdvertisement.party_size;
    party->hero_count = event->PartySearchAdvertisement.hero_count;
    party->search_type = event->PartySearchAdvertisement.search_type;
    party->hardmode = event->PartySearchAdvertisement.hardmode;
    party->district_number = event->PartySearchAdvertisement.district_number;
    party->language = event->PartySearchAdvertisement.language;
    party->primary = event->PartySearchAdvertisement.primary;
    party->secondary = event->PartySearchAdvertisement.secondary;
    party->level = event->PartySearchAdvertisement.level;

    if (event->PartySearchAdvertisement.message.length) {
        int written = uint16_to_char(event->PartySearchAdvertisement.message.buffer, party->message, ARRAY_SIZE(party->message));
        assert(written > 0);
    }
    else {
        *party->message = 0;
    }
    if (event->PartySearchAdvertisement.sender.length) {
        int written = uint16_to_char(event->PartySearchAdvertisement.sender.buffer, party->sender, ARRAY_SIZE(party->sender));
        assert(written > 0);
    }
    else {
        *party->sender = 0;
    }

    return party;
}

static void send_bot_info() {
    if (!(account_uuid && *account_uuid))
        return;
    // When websocket handshake is successful, send message
    json_t* json = json_object();
    json_object_set_new(json, "type", json_string("client_info"));
    json_object_set_new( json, "client_id", json_string( account_uuid ) );
    char* json_str = json_dumps(json, JSON_COMPACT | JSON_ENSURE_ASCII);
    assert(json_str);
    json_delete(json);

    CURL* curl = 0;
    curl = init_curl(server_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    CURLcode res = curl_easy_perform(curl);

    if(curl) curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LogError("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
        LogInfo("[Sent OK]");
        last_successful_curl = time_get_ms();
    }

    free(json_str);
}
static void send_party_search_advertisements(bool force) {
    json_t* json_obj = NULL;
    char* json_str = NULL;

    DistrictRegion district_region;
    DistrictLanguage district_language;
    GetDistrict(&district_region,&district_language);
    int district_number = GetDistrictNumber();
    int map_id = GetMapId();

    json_obj = json_object();
    json_object_set_new(json_obj, "type", json_string("client_parties"));
    json_object_set_new( json_obj, "map_id", json_integer( map_id ) );
    json_object_set_new( json_obj, "account_uuid", json_string( account_uuid ) );
    json_object_set_new( json_obj, "district_region", json_integer( district_region ) );
    json_object_set_new( json_obj, "district_number", json_integer( district_number ));
    json_object_set_new( json_obj, "district_language", json_integer(district_language) );
    json_object_set_new( json_obj, "client_id", json_string(account_uuid) );
    

    json_t *json_parties = json_array();
    json_object_set_new( json_obj, "parties", json_parties );

    PartySearchAdvertisement* party;

    array_foreach(party, &party_search_advertisements) {
        if (!(party && party->party_id))
            continue;
        json_array_append( json_parties, party_search_advertisement_to_json(party) );
    }

    json_str = json_dumps(json_obj, JSON_COMPACT | JSON_ENSURE_ASCII);
    assert(json_str);

    json_delete(json_obj);
    json_obj = NULL;

    CURL* curl = 0;
    curl = init_curl(server_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    CURLcode res = curl_easy_perform(curl);

    if(curl) curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        LogError("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
        LogInfo("[Sent OK]");
        last_successful_curl = time_get_ms();
    }

    if (json_str)
        free(json_str);
    if (json_obj)
        json_delete(json_obj);
}

static void on_map_left(Event* event, void* params) {
    remove_all_party_search_advertisements(false);
}
static void on_map_entered(Event* event, void* params) {
    pending_send_party_search_advertisements = true;
}

static void add_party_search_advertisement(Event* event, void* params) {
    assert(event && event->type == EventType_PartySearchAdvertisement && event->PartySearchAdvertisement.party_id);

    create_party_search_advertisement(event);

    pending_send_party_search_advertisements = true;
}
static void update_party_search_advertisement(Event* event, void* params) {
    assert(event && event->PartySearchAdvertisement.party_id);

    PartySearchAdvertisement* party = get_party_search_advertisement(event->PartySearchAdvertisement.party_id);
    if (party) {
        switch (event->type) {
        case EventType_PartySearchSize:
            party->party_size = event->PartySearchAdvertisement.party_size;
            party->hero_count = event->PartySearchAdvertisement.hero_count;
            pending_send_party_search_advertisements = true;
            break;
        case EventType_PartySearchType:
            party->search_type = event->PartySearchAdvertisement.search_type;
            party->hardmode = event->PartySearchAdvertisement.hardmode;
            pending_send_party_search_advertisements = true;
            break;
        case EventType_PartySearchRemoved:
            party->party_id = 0;
            pending_send_party_search_advertisements = true;
            break;
        }
    }
}

static void exit_with_status(const char* state, int exit_code)
{
    LogCritical("%s (code=%d)", state, exit_code);
    exit(exit_code);
}

static bool in_correct_outpost() {
    if (!GetIsIngame())
        return false;
    int map_id = GetMapId();
    bool valid = map_id == required_map_id;
    switch (map_id) {
    case MapID_Ascalon:
        valid = required_map_id == MapID_Ascalon;
        break;
    case MapID_Kamadan:
    case MapID_Kamadan_Halloween:
    case MapID_Kamadan_Wintersday:
    case MapID_Kamadan_Canthan_New_Year:
        valid = required_map_id == MapID_Kamadan;
        break;
    }
    return valid && GetDistrict(NULL, NULL) == required_district && (!required_district_number || GetDistrictNumber() == required_district_number);
}

// Based on current character, choose which preferred district/map
static void set_runtime_vars() {
    // Not applicable
}

static int main_bot(void* param)
{

    running = true;

    array_init(&party_search_advertisements);

    sprintf(server_url, "%s/api",server_domain); // Can change later maybe

    CallbackEntry_Init(&EventType_WorldMapLeave_entry, on_map_left, NULL);
    RegisterEvent(EventType_WorldMapLeave, &EventType_WorldMapLeave_entry);

    CallbackEntry_Init(&EventType_WorldMapEnter_entry, on_map_entered, NULL);
    RegisterEvent(EventType_WorldMapEnter, &EventType_WorldMapEnter_entry);

    CallbackEntry_Init(&EventType_PartySearchAdvertisement_entry, add_party_search_advertisement, NULL);
    RegisterEvent(EventType_PartySearchAdvertisement, &EventType_PartySearchAdvertisement_entry);

    CallbackEntry_Init(&EventType_PartySearchRemoved_entry, update_party_search_advertisement, NULL);
    RegisterEvent(EventType_PartySearchRemoved, &EventType_PartySearchRemoved_entry);

    CallbackEntry_Init(&EventType_PartySearchSize_entry, update_party_search_advertisement, NULL);
    RegisterEvent(EventType_PartySearchSize, &EventType_PartySearchSize_entry);

    CallbackEntry_Init(&EventType_PartySearchType_entry, update_party_search_advertisement, NULL);
    RegisterEvent(EventType_PartySearchType, &EventType_PartySearchType_entry);

    LogInfo("HQ hooks done");

    long wait = 0;
    long wait_timeout_seconds = 60;
    LogInfo("Waiting for game load");
    while (!GetIsIngame()) {
        if (wait > 1000 * wait_timeout_seconds) {
            exit_with_status("Failed to wait for ingame after 10s", 1);
        }
        wait += 500;
        time_sleep_ms(500);
    }
    time_sleep_ms(2000);

    int player_name_len = 0;
    msec_t deadlock = time_get_ms() + 5000;
    while (!(player_name_len = GetCharacterName(player_name_utf8, ARRAY_SIZE(player_name_utf8)))) {
        if (time_get_ms() > deadlock) {
            LogError("Couldn't get current character name");
            goto cleanup;
        }
        time_sleep_ms(50);
    };
    player_name_utf8[player_name_len] = 0;

    int ok = GetAccountUuid(account_uuid, ARRAY_SIZE(account_uuid));
    if (ok < 10) {
        LogError("Failed to get account uuid");
        goto cleanup;
    }
    LogInfo("Account UUID is %s", account_uuid);

    send_bot_info();
    
    assert(player_name_len < ARRAY_SIZE(player_name));
    for (int i = 0; i < player_name_len; i++) {
        if (player_name_utf8[i] == 0)
            continue;
        player_name[i] = player_name_utf8[i];
    }
    player_name[player_name_len] = 0;
    LogInfo("Player name is %ls", player_name);
    set_runtime_vars();

    uint64_t elapsed_seconds = time_get_ms() / 1000;
    int not_in_game_tally = 0;
    unsigned int sleep_ms = 500;
    //unsigned int stale_ping_s = 120; // Seconds to wait before sending message to myself to ensure chat connection
    while (running) {
        time_sleep_ms(sleep_ms);
        if (!GetIsIngame() || !GetIsConnected()) {
            not_in_game_tally++;
            if (not_in_game_tally * sleep_ms > 5000) {
                LogError("GetIsIngame() returned false for 5 seconds, client may have disconnected");
                goto cleanup;
            }
            continue;
        }
        if (!in_correct_outpost()) {
            LogInfo("Zoning into outpost");
            int res = 0;
            size_t retries = 4;
            for (size_t i = 0; i < retries && !in_correct_outpost(); i++) {
                LogInfo("Travel attempt %d of %d", i + 1, retries);
                res = travel_wait(required_map_id, required_district, required_district_number);
                if (res == 38) {
                    retries = 50; // District full, more retries
                }
            }
            if (!in_correct_outpost()) {
                exit_with_status("Couldn't travel to outpost", 1);
            }
            LogInfo("I should be in outpost %d %d %d, listening for chat messages", GetMapId(), GetDistrict(NULL, NULL), GetDistrictNumber());
        }
        not_in_game_tally = 0;
        elapsed_seconds = time_get_ms() / 1000;
        if (pending_send_party_search_advertisements) {
            pending_send_party_search_advertisements = false;
            send_party_search_advertisements(true);    
        }
    }
cleanup:
    UnRegisterEvent(&EventType_PartySearchAdvertisement_entry);
    UnRegisterEvent(&EventType_PartySearchRemoved_entry);
    UnRegisterEvent(&EventType_PartySearchSize_entry);
    UnRegisterEvent(&EventType_PartySearchType_entry);
    UnRegisterEvent(&EventType_WorldMapEnter_entry);
    UnRegisterEvent(&EventType_WorldMapLeave_entry);

    remove_all_party_search_advertisements(true);

    array_reset(&party_search_advertisements);

    raise(SIGTERM);
    return 0;
}
DllExport void PluginUnload(PluginObject* obj)
{
    running = false;
}
DllExport bool PluginEntry(PluginObject* obj)
{
    thread_create(&bot_thread, main_bot, NULL);
    thread_detach(&bot_thread);
    obj->PluginUnload = &PluginUnload;
    return true;
}

DllExport void on_panic(const char *msg)
{
    exit_with_status("Error", 1);
}
int main(void) {
    printf("Hello world");
    return 0;
}
