#include "fmo_controls.h"
#include "fmo_wifi_qr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    fmo_controls_t c = {0};
    assert(fmo_controls_key(&c, FMO_KEY_UP) == FMO_ACTION_BRIGHTER);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_REFRESH);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_NONE);
    assert(c.view == FMO_VIEW_NETWORK && c.selection == 0);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_SETUP);
    fmo_controls_key(&c, FMO_KEY_UP); assert(c.selection == 2);
    fmo_controls_key(&c, FMO_KEY_DOWN); assert(c.selection == 0);
    fmo_controls_key(&c, FMO_KEY_DOWN);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_RETRY);
    assert(c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, true);
    assert(c.view == FMO_VIEW_SETUP && !c.setup_info);
    fmo_controls_key(&c, FMO_KEY_OK); assert(c.setup_info);
    fmo_controls_key(&c, FMO_KEY_OK); assert(!c.setup_info);
    assert(fmo_controls_key(&c, FMO_KEY_UP) == FMO_ACTION_NONE);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_CANCEL);
    fmo_controls_observe_setup(&c, true); assert(c.view == FMO_VIEW_NETWORK);
    fmo_controls_observe_setup(&c, false); assert(c.view == FMO_VIEW_NETWORK);
    fmo_controls_key(&c, FMO_KEY_BACK); assert(c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, true); fmo_controls_observe_setup(&c, false);
    assert(c.view == FMO_VIEW_MONITOR);
    char qr[160];
    assert(fmo_wifi_qr_payload(qr, sizeof(qr), "FMO-Setup-TEST", "ABCDEF012345"));
    assert(!strcmp(qr, "WIFI:T:WPA;S:FMO-Setup-TEST;P:ABCDEF012345;;"));
    assert(fmo_wifi_qr_payload(qr, sizeof(qr), "a;b,c:d", "\\q\""));
    assert(!strcmp(qr, "WIFI:T:WPA;S:a\\;b\\,c\\:d;P:\\\\q\\\";;"));
    size_t length = strlen(qr);
    assert(fmo_wifi_qr_payload(qr, length+1, "a;b,c:d", "\\q\""));
    assert(!fmo_wifi_qr_payload(qr, length, "a;b,c:d", "\\q\"") && !qr[0]);
    assert(!fmo_wifi_qr_payload(qr, sizeof(qr), "", "pw"));
    assert(!fmo_wifi_qr_payload(qr, sizeof(qr), "abc\n", "pw") && !qr[0]);
    char guard[2] = {'x','y'};
    assert(!fmo_wifi_qr_payload(guard, 1, "ssid", "pw"));
    assert(!guard[0] && guard[1] == 'y');
    assert(!fmo_wifi_qr_payload(NULL, 5, "ssid", "pw"));
    puts("FMO controls and Wi-Fi QR: PASS");
}
