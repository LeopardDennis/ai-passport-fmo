#include "fmo_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static fmo_pcm_t pcm;
static int16_t output[FMO_PCM_CHUNK_SAMPLES];
static uint8_t frame[5120];
static void pattern(void)
{
    for (size_t i = 0; i < sizeof(frame) / 2; ++i) {
        int16_t v = (int16_t)((int)i - 1280);
        frame[2*i]=(uint8_t)v; frame[2*i+1]=(uint8_t)((uint16_t)v >> 8);
    }
}
static void feed_frame(uint64_t now)
{
    for (size_t offset=0; offset<sizeof(frame); offset+=1024) {
        fmo_pcm_result_t result=fmo_pcm_feed(&pcm,2,true,sizeof(frame),offset,frame+offset,1024,now);
        assert(result==(offset+1024==sizeof(frame) ? FMO_PCM_COMPLETE : FMO_PCM_MORE));
    }
}
int main(void)
{
    pattern(); fmo_pcm_reset(&pcm);
    feed_frame(1000); /* Real FMO frame shape: 5120 bytes in five receive events. */
    assert(pcm.count==2560 && !pcm.active);
    for (size_t block=0; block<16; ++block) {
        assert(fmo_pcm_read(&pcm,output,160,1000+block*20)==160);
        for(size_t i=0;i<160;++i)assert(output[i]==(int16_t)((int)(block*160+i)-1280));
    }
    assert(fmo_pcm_read(&pcm,output,160,1320)==0);
    for(size_t i=0;i<160;++i)assert(output[i]==0); /* Underflow produces silence. */
    fmo_pcm_reset(&pcm);
    const uint8_t signed_samples[]={0x00,0x80,0xff,0x7f,0xff,0xff,0,0};
    assert(fmo_pcm_feed(&pcm,2,false,3,0,signed_samples,1,2000)==FMO_PCM_MORE);
    assert(fmo_pcm_feed(&pcm,9,true,0,0,NULL,0,2000)==FMO_PCM_IGNORED);
    assert(fmo_pcm_feed(&pcm,2,false,3,1,signed_samples+1,2,2000)==FMO_PCM_MORE);
    assert(fmo_pcm_feed(&pcm,0,true,5,0,signed_samples+3,5,2000)==FMO_PCM_COMPLETE);
    assert(fmo_pcm_read(&pcm,output,160,2059)==0); /* Short tail waits briefly. */
    assert(fmo_pcm_read(&pcm,output,160,2060)==4);
    assert(output[0]==INT16_MIN && output[1]==INT16_MAX && output[2]==-1 && output[3]==0);
    assert(output[4]==0);
    /* A partial sample, invalid offset or unexpected continuation cannot poison
     * the next talker's byte alignment or retain old queued audio. */
    feed_frame(3000);
    assert(fmo_pcm_feed(&pcm,2,true,1,0,signed_samples,1,3000)==FMO_PCM_INVALID);
    assert(!pcm.count && !pcm.partial_sample);
    assert(fmo_pcm_feed(&pcm,0,true,2,0,signed_samples,2,3000)==FMO_PCM_INVALID);
    assert(fmo_pcm_feed(&pcm,2,true,6,1,signed_samples,2,3000)==FMO_PCM_INVALID);
    assert(fmo_pcm_feed(&pcm,2,true,16001,0,signed_samples,2,3000)==FMO_PCM_INVALID);
    assert(fmo_pcm_feed(&pcm,2,true,6,0,NULL,2,3000)==FMO_PCM_INVALID);
    assert(fmo_pcm_feed(&pcm,1,true,4,0,"text",4,3000)==FMO_PCM_IGNORED);
    assert(fmo_pcm_feed(&pcm,2,true,2,0,signed_samples,2,3000)==FMO_PCM_COMPLETE);
    assert(fmo_pcm_read(&pcm,output,160,3060)==1 && output[0]==INT16_MIN);
    /* Slow playback is bounded and drops oldest samples, never half samples. */
    fmo_pcm_reset(&pcm); feed_frame(4000); feed_frame(4010);
    assert(pcm.count==FMO_PCM_CAPACITY);
    assert(fmo_pcm_read(&pcm,output,160,4010)==160);
    assert(output[0]==-256); /* Dropped 1024 oldest samples. */
    fmo_pcm_reset(&pcm);
    assert(fmo_pcm_read(&pcm,output,160,4020)==0 && output[0]==0);
    puts("FMO PCM: PASS (live frame size, chunk splits, signed PCM, buffering, overflow, reset)");
}
