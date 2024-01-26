#ifdef GW_HELPER_C_INC
#error "gw-helper.c is already included"
#endif
#define GW_HELPER_C_INC

#include <float.h> // FLT_EPSILON
#include <common/time.h>
#include <common/macro.h>

#include <signal.h>
#include <stdio.h>

#include "async.h"

static int   irand(int min, int max);
static float frand(float min, float max);
static float dist2(Vec2f u, Vec2f v);
static bool  equ2f(Vec2f v1, Vec2f v2);
static Vec2f lerp2f(Vec2f a, Vec2f b, float t);

#define sec_to_ms(sec) (sec * 1000)

// Ensure uint16_t* is null terminated, adding one if needed. Returns length of uint16_t string excluding null terminator.
size_t null_terminate_uint16(uint16_t* buffer, size_t buffer_len) {
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}
// Ensure wchar_t* is null terminated, adding one if needed. Returns length of wchar_t string excluding null terminator.
size_t null_terminate_wchar(wchar_t* buffer, size_t buffer_len) {
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}
// Ensure char* is null terminated, adding one if needed. Returns length of char string excluding null terminator.
size_t null_terminate_char(char* buffer, size_t buffer_len) {
    //LogDebug("null_terminate_char: %p, %d", buffer, buffer_len);
    for (size_t i = 0; i < buffer_len - 1; i++) {
        if (buffer[i] == 0)
            return i;
    }
    buffer[buffer_len - 1] = 0;
    return buffer_len - 1;
}

// Returns length of char string excluding null terminator.
// from_buffer MUST be a null terminated string.
static int wchar_to_char(const wchar_t* from_buffer, char* to_buffer, size_t to_buffer_length) {
    setlocale(LC_ALL, "en_US.utf8");
#pragma warning(suppress : 4996)
#if 0
    //wcstombs(pIdentifier->Description, desc.Description, MAX_DEVICE_IDENTIFIER_STRING);
    int len = WideCharToMultiByte(CP_UTF8, 0, from, -1, NULL, 0, NULL, NULL);
    if (len == 0 || len > (int)max_len)
        return -1;
    WideCharToMultiByte(CP_UTF8, 0, from, -1, to, len, NULL, NULL);
    return (int)null_terminate_char(to, max_len);
#else
    int from_buffer_len = wcslen(from_buffer);
    if (to_buffer_length < from_buffer_len * sizeof(wchar_t)) {
        LogError("wchar_to_char: Out buffer length %d is not big enough; need %d", to_buffer_length, from_buffer_len * sizeof(wchar_t));
        return -1; // char array should be twice the length of the from_buffer
    }
    //LogDebug("wchar_to_char: wcstombs(%p, %p, %d)", to_buffer, from_buffer, to_buffer_length);
    int len = (int)wcstombs(to_buffer, from_buffer, to_buffer_length);
    if (len < 0) {
        LogError("wchar_to_char: wcstombs failed %d", len);
        return len;
    }
    return (int)null_terminate_char(to_buffer, to_buffer_length);
#endif
}
// Returns length of wchar_t string excluding null terminator.
static int uint16_to_wchar(const uint16_t* from_buffer, wchar_t* to_buffer, size_t to_buffer_length) {
    size_t written = 0;
    to_buffer[0] = 0;
    for (written = 0; written < to_buffer_length - 1; written++) {
        to_buffer[written] = from_buffer[written];
        if (!from_buffer[written])
            break;
    }
    return (int)null_terminate_wchar(to_buffer, to_buffer_length);
}
// Returns length of char string excluding null terminator. to_buffer is null terminated.
static int uint16_to_char(const uint16_t* from_buffer, char* to_buffer, const size_t to_buffer_length) {
    int result = -1;
    if (!to_buffer_length)
        return result;
    size_t tmp_to_buffer_bytes = to_buffer_length * sizeof(wchar_t);
    wchar_t* tmp_to_buffer = malloc(tmp_to_buffer_bytes);
    result = uint16_to_wchar(from_buffer, tmp_to_buffer, to_buffer_length);
    if (result < 1)
        goto cleanup;
    // Result is null terminated now.
    result = wchar_to_char(tmp_to_buffer, to_buffer, to_buffer_length);
cleanup:
    free(tmp_to_buffer);
    return result;
}
static bool str_match_uint32_t(uint32_t* first, uint32_t* second) {
    size_t len1 = 0;
    while (first[len1]) len1++;
    size_t len2 = 0;
    while (first[len2]) len2++;
    if (len1 != len2)
        return false;
    for (size_t i = 0; i < len1; i++) {
        if (first[i] != second[i])
            return false;
    }
    return true;
}
static bool str_match_uint16_t(uint16_t* first, uint16_t* second) {
    size_t len1 = 0;
    while (first[len1]) len1++;
    size_t len2 = 0;
    while (first[len2]) len2++;
    if (len1 != len2)
        return false;
    for (size_t i = 0; i < len1; i++) {
        if (first[i] != second[i])
            return false;
    }
    return true;
}

static int wait_map_loading(int map_id, msec_t timeout_ms);
static int travel_wait(int map_id, District district, uint16_t district_number)
{
    if ((GetMapId() == map_id) && (GetDistrict(NULL,NULL) == district)
        && (!district_number || GetDistrictNumber() == district_number)) {
        return 0;
    }
    Travel(map_id, district, district_number);
    return wait_map_loading(map_id, 20000);
}

static int wait_map_loading(int map_id, msec_t timeout_ms)
{
    AsyncState state;
    async_wait_map_loading(&state, timeout_ms);

    unsigned int current_ms = 0;
    while (!async_check(&state)) {
        time_sleep_ms(16);
    }
    if (state.result != ASYNC_RESULT_OK) {
        return state.result;
    }
    // @Cleanup:
    // We shouldn't have that here.
    current_ms = 0;
    while (!GetMyAgentId()) {
        time_sleep_ms(current_ms += 16);
        if (current_ms > (int)timeout_ms) {
            return ASYNC_RESULT_TIMEOUT; // Timeout
        }
    }

    return GetMapId() == map_id ? ASYNC_RESULT_OK : ASYNC_RESULT_WRONG_VALUE;
}
