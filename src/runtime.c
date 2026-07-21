#include "dfx.h"

#include <signal.h>
#include <string.h>

static volatile sig_atomic_t cancelled = 0;

void dfx_request_cancel(void) {
    cancelled = 1;
}

bool dfx_cancel_requested(void) {
    return cancelled != 0;
}

bool dfx_lab_mode_available(void) {
#ifdef DFX_ENABLE_LAB_MODE
    return true;
#else
    return false;
#endif
}

bool dfx_text_is_safe(const char *text) {
    if (text == NULL) return false;
    const unsigned char *bytes = (const unsigned char *)text;
    size_t length = strlen(text);
    size_t index = 0;
    while (index < length) {
        unsigned char value = bytes[index];
        if (value < 0x20U || value == 0x7fU) return false;
        if (value < 0x80U) {
            index++;
            continue;
        }
        size_t count = 0;
        if (value >= 0xc2U && value <= 0xdfU && index + 1 < length && bytes[index + 1] >= 0x80U && bytes[index + 1] <= 0xbfU) count = 2;
        else if (value == 0xe0U && index + 2 < length && bytes[index + 1] >= 0xa0U && bytes[index + 1] <= 0xbfU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU) count = 3;
        else if (((value >= 0xe1U && value <= 0xecU) || (value >= 0xeeU && value <= 0xefU)) && index + 2 < length && bytes[index + 1] >= 0x80U && bytes[index + 1] <= 0xbfU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU) count = 3;
        else if (value == 0xedU && index + 2 < length && bytes[index + 1] >= 0x80U && bytes[index + 1] <= 0x9fU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU) count = 3;
        else if (value == 0xf0U && index + 3 < length && bytes[index + 1] >= 0x90U && bytes[index + 1] <= 0xbfU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU && bytes[index + 3] >= 0x80U && bytes[index + 3] <= 0xbfU) count = 4;
        else if (value >= 0xf1U && value <= 0xf3U && index + 3 < length && bytes[index + 1] >= 0x80U && bytes[index + 1] <= 0xbfU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU && bytes[index + 3] >= 0x80U && bytes[index + 3] <= 0xbfU) count = 4;
        else if (value == 0xf4U && index + 3 < length && bytes[index + 1] >= 0x80U && bytes[index + 1] <= 0x8fU && bytes[index + 2] >= 0x80U && bytes[index + 2] <= 0xbfU && bytes[index + 3] >= 0x80U && bytes[index + 3] <= 0xbfU) count = 4;
        if (count == 0) return false;
        index += count;
    }
    return true;
}

static unsigned char ascii_lower(unsigned char value) {
    if (value >= 'A' && value <= 'Z') return (unsigned char)(value + ('a' - 'A'));
    return value;
}

static bool ascii_equal_case_insensitive(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (ascii_lower((unsigned char)*left) != ascii_lower((unsigned char)*right)) return false;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

bool dfx_hardware_id_is_credible(const char *text) {
    if (!dfx_text_is_safe(text)) return false;
    size_t length = strlen(text);
    if (length < 4 || length >= DFX_ID_MAX || text[0] == ' ' || text[length - 1] == ' ') return false;
    static const char *rejected[] = {
        "unknown", "none", "n/a", "null", "default", "serial", "serialnumber", "not specified", "to be filled by o.e.m.", "00000000", "0123456789", "123456789"
    };
    for (size_t index = 0; index < sizeof(rejected) / sizeof(rejected[0]); index++) {
        if (ascii_equal_case_insensitive(text, rejected[index])) return false;
    }
    unsigned alphanumeric = 0;
    unsigned char first = 0;
    bool distinct = false;
    for (size_t index = 0; index < length; index++) {
        unsigned char value = (unsigned char)text[index];
        bool digit = value >= '0' && value <= '9';
        bool letter = (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        if (!digit && !letter) continue;
        value = ascii_lower(value);
        if (alphanumeric == 0) first = value;
        else if (value != first) distinct = true;
        alphanumeric++;
    }
    return alphanumeric >= 4 && distinct;
}
