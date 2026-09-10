#include "fmo_credentials.h"
#include <assert.h>
#include <string.h>
int main(void) {
    assert(fmo_credentials_valid("wifi",4,"",0));
    assert(fmo_credentials_valid("wifi",4,"12345678",8));
    assert(!fmo_credentials_valid("",0,"",0));
    assert(!fmo_credentials_valid("wifi",4,"short",5));
    char ssid[33]; memset(ssid,'x',sizeof(ssid));
    char pass[64]; memset(pass,'y',sizeof(pass));
    assert(fmo_credentials_valid(ssid,32,pass,63));
    assert(!fmo_credentials_valid(ssid,33,pass,63));
    assert(!fmo_credentials_valid(ssid,32,pass,64));
    assert(!fmo_credentials_valid("a\0b",3,"",0));
    assert(!fmo_credentials_valid(NULL,1,"",0));
    return 0;
}
