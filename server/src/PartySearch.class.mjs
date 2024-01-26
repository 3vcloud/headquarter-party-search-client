import {assert} from "./assert.mjs";
import {
    district_regions,
    getDistrictAbbreviation,
    getDistrictName, getMapInfo,
    getMapName,
    languages, map_info,
    party_search_types
} from "./gw_constants.mjs";
import {to_number} from "./string_functions.mjs";

export class PartySearch {
    constructor(json) {
        this.client_id = json.client_id || '';
        this.message = json.message || '';
        this.sender = json.sender || '';
        this.party_id = to_number(json.party_id);
        this.party_size = to_number(json.party_size);
        this.hero_count = to_number(json.hero_count);
        this.search_type = to_number(json.search_type);
        this.district_number = to_number(json.district_number || 0);
        this.district_region = to_number(json.district_region || 0);
        this.district_language = to_number(json.district_language || json.language || 0);
        this.map_id = to_number(json.map_id || 0);

        this.validate();
    }
    validate() {
        assert(typeof this.client_id === 'string' && this.client_id.length);
        assert(typeof this.message === 'string');
        assert(typeof this.sender === 'string' && this.sender.length);
        assert(typeof this.party_id === 'number' && this.party_size > 0);
        assert(typeof this.party_size === 'number' && this.party_size > 0);
        assert(typeof this.hero_count === 'number' && this.hero_count < this.party_size);
        assert(typeof this.map_id === 'number' && this.map_id > 0 && map_info[this.map_id]);
        assert(typeof this.district_number === 'number');
        assert(typeof this.search_type === 'number' && party_search_types[this.search_type]);
        assert(typeof this.district_region === 'number' && district_regions[this.district_region] );
        assert(typeof this.district_language === 'number' && languages[this.district_language]);
    }

    get map_name() {
        return getMapName(this.map_id);
    }
    get map_info() {
        return getMapInfo(this.map_id);
    }
    get district_abbr() {
        return `${getDistrictAbbreviation(this.district_region,this.district_language)} - ${this.district_number}`;
    }
    get district_name() {
        return `${getDistrictName(this.district_region)}, district ${this.district_number}`
    }
    get search_type_name() {
        return party_search_types[this.search_type];
    }
}
