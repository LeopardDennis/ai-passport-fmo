#include "fmo_controls.h"
#include "fmo_wifi_qr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    fmo_controls_t c = {.audio_enabled=true,.volume=50};
    assert(fmo_controls_key(&c, FMO_KEY_UP) == FMO_ACTION_VOLUME);
    assert(c.volume==60);
    assert(fmo_controls_key(&c, FMO_KEY_DOWN) == FMO_ACTION_VOLUME && c.volume==50);
    c.volume=95; fmo_controls_key(&c,FMO_KEY_UP); assert(c.volume==100);
    c.volume=5; fmo_controls_key(&c,FMO_KEY_DOWN); assert(c.volume==0);
    c.volume=50;
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_AUDIO && !c.audio_enabled);
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_AUDIO && c.audio_enabled);
    assert(fmo_controls_key(&c, FMO_KEY_REFRESH) == FMO_ACTION_REFRESH);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_NONE);
    assert(c.view == FMO_VIEW_NETWORK && c.selection == 0);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_SETUP);
    fmo_controls_key(&c, FMO_KEY_UP); assert(c.selection == 2);
    fmo_controls_key(&c, FMO_KEY_DOWN); assert(c.selection == 0);
    fmo_controls_key(&c, FMO_KEY_DOWN);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_NONE);
    assert(c.view == FMO_VIEW_WIFI);
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_NONE); // Empty list does not connect.
    fmo_wifi_list_t wifi={.count=5,.names={"home","office","mobile","cafe","lab"}};
    fmo_controls_observe_wifi(&c,&wifi);
    for(unsigned i=0;i<5;++i) {
        fmo_controls_key(&c,FMO_KEY_DOWN);assert(c.selection==(i+1)%5);
        fmo_controls_observe_wifi(&c,&wifi);assert(c.selection==(i+1)%5);
    }
    fmo_controls_key(&c,FMO_KEY_UP);assert(c.selection==4);
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_RETRY && c.view==FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, true);
    assert(c.view == FMO_VIEW_NETWORK && c.selection == 0 && !c.setup_info);
    for (unsigned i = 0; i < 3; ++i) {
        fmo_controls_key(&c, FMO_KEY_DOWN);
        assert(c.selection == (i + 1) % 3);
        fmo_controls_observe_setup(&c, true); // Repeated snapshots must not reset selection.
        assert(c.view == FMO_VIEW_NETWORK && c.selection == (i + 1) % 3);
    }
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_NONE && c.view == FMO_VIEW_SETUP);
    fmo_controls_key(&c, FMO_KEY_OK); assert(c.setup_info);
    fmo_controls_key(&c, FMO_KEY_OK); assert(!c.setup_info);
    assert(fmo_controls_key(&c, FMO_KEY_UP) == FMO_ACTION_CANCEL);
    assert(c.view == FMO_VIEW_NETWORK);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_NONE && c.view == FMO_VIEW_SETUP);
    assert(fmo_controls_key(&c, FMO_KEY_DOWN) == FMO_ACTION_CANCEL);
    assert(c.view == FMO_VIEW_NETWORK);
    fmo_controls_key(&c, FMO_KEY_OK);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_CANCEL);
    fmo_controls_observe_setup(&c, true); assert(c.view == FMO_VIEW_NETWORK);
    fmo_controls_observe_setup(&c, false); assert(c.view == FMO_VIEW_NETWORK);
    fmo_controls_key(&c, FMO_KEY_BACK); assert(c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, true);
    fmo_controls_key(&c, FMO_KEY_DOWN);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_NONE && c.view == FMO_VIEW_WIFI);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_NONE && c.view == FMO_VIEW_NETWORK && c.selection==1);
    fmo_controls_key(&c, FMO_KEY_OK);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_RETRY && c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, false);
    fmo_controls_observe_setup(&c, true);
    fmo_controls_key(&c, FMO_KEY_UP);
    assert(c.selection == 2);
    assert(fmo_controls_key(&c, FMO_KEY_OK) == FMO_ACTION_CANCEL && c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, false);
    fmo_controls_observe_setup(&c, true);
    assert(fmo_controls_key(&c, FMO_KEY_BACK) == FMO_ACTION_CANCEL && c.view == FMO_VIEW_MONITOR);
    fmo_controls_observe_setup(&c, false);
    fmo_controls_observe_setup(&c, true);
    fmo_controls_key(&c, FMO_KEY_OK);
    fmo_controls_observe_setup(&c, false);
    assert(c.view == FMO_VIEW_MONITOR);
    c.view=FMO_VIEW_WIFI;c.selection=4;
    fmo_wifi_list_t changed={.count=2,.names={"lab","home"}};
    fmo_controls_observe_wifi(&c,&changed);assert(c.selection==0 && !strcmp(c.wifi.names[0],"lab"));
    changed.count=1;strcpy(changed.names[0],"home");
    fmo_controls_observe_wifi(&c,&changed);assert(c.selection==1);
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_NONE && c.view==FMO_VIEW_WIFI);
    fmo_controls_key(&c,FMO_KEY_DOWN);assert(c.selection==0);
    changed.count=0;fmo_controls_observe_wifi(&c,&changed);
    assert(c.selection==0 && fmo_controls_key(&c,FMO_KEY_DOWN)==FMO_ACTION_NONE);
    assert(fmo_controls_key(&c,FMO_KEY_OK)==FMO_ACTION_NONE);
    assert(fmo_controls_key(&c,FMO_KEY_BACK)==FMO_ACTION_NONE && c.view==FMO_VIEW_NETWORK);
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
