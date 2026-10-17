#pragma once
/* Start once after application NVS/install cleanup. Lifetime-owned worker;
 * no LVGL calls and no HTTP/storage work on the audio/event/UI tasks. */
void fmo_geocoder_start(void);
