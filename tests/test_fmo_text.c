#include "fmo_text.h"
#include "fmo_monitor_state.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    assert(fmo_text_json_has_nul("\\u0000"));
    assert(fmo_text_json_has_nul("x\\u0000y"));
    assert(!fmo_text_json_has_nul("\\\\u0000"));
    assert(fmo_text_json_has_nul("\\\\\\u0000"));
    assert(!fmo_text_json_has_nul("\\u000"));
    assert(!fmo_text_json_has_nul(NULL));
    char text[32];
    assert(fmo_text_copy_utf8(text, sizeof(text), "安吉FMO中继"));
    assert(!strcmp(text, "安吉FMO中继"));
    assert(fmo_text_copy_utf8(text, 5, "A上海"));
    assert(!strcmp(text, "A上"));
    assert(fmo_text_copy_utf8(text, 4, "上海"));
    assert(!strcmp(text, "上"));
    assert(fmo_text_copy_utf8(text, 3, "上海"));
    assert(!strcmp(text, ""));
    assert(fmo_text_copy_utf8(text, 5, "\xf0\x9f\x93\xbbX"));
    assert(strlen(text) == 4);
    assert(fmo_text_copy_utf8(text, 3, "\xc3\xa9X"));
    assert(strlen(text) == 2);
    const char *invalid[] = {"\x80", "\xc0\xaf", "\xe0\x80\xaf", "\xed\xa0\x80",
        "\xf4\x90\x80\x80", "\xf0\x80\x80\xaf", "\xe4\xb8", "\xf0",
        "X\n", "\xc2\x80", "\x7f", "long-invalid-suffix\xff"};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        assert(!fmo_text_copy_utf8(text, 3, invalid[i]));
        assert(!text[0]);
    }
    assert(!fmo_text_copy_utf8(NULL, 3, "ok"));
    assert(!fmo_text_copy_utf8(text, 0, "ok"));
    assert(!fmo_text_copy_utf8(text, sizeof(text), NULL));
    struct { char small[4]; char guard; } bounded = {{0}, 'Q'};
    assert(fmo_text_copy_utf8(bounded.small, 4, "上海FMO"));
    assert(!strcmp(bounded.small, "上") && bounded.guard == 'Q');
    fmo_monitor_state_t state = {0};
    fmo_monitor_set_channel(&state, 42, "一二三四五六七八九十十一十二十三");
    assert(!strcmp(state.channel_name, "一二三四五六七八九十"));
    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM01", true, false, false, 100);
    fmo_monitor_set_channel(&state, 42, "安吉FMO中继");
    assert(state.speaking && !strcmp(state.channel_name, "安吉FMO中继"));
    fmo_monitor_set_channel(&state, 43, "上海");
    assert(!state.speaking && !state.last_speaker[0]);
    return 0;
}
