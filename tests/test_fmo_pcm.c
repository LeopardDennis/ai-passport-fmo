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
/* Vary odd/unaligned receive fragments while draining committed audio. Track
 * a reference sample sequence through both ring wraps and bounded overflow. */
static void verify_ring_stream(void)
{
    fmo_pcm_reset(&pcm);
    for (unsigned i = 0; i < 6; ++i) feed_frame(1000 + i);
    size_t next = pcm.dropped_samples;
    uint32_t dropped = pcm.dropped_samples, random = 17;
    for (unsigned message = 0; message < 80; ++message) {
        size_t offset = 0;
        uint64_t now = 2000 + message * 320;
        while (offset < sizeof(frame)) {
            random = random * 1664525U + 1013904223U;
            size_t chunk = 1 + random % 1024;
            if (chunk > sizeof(frame) - offset) chunk = sizeof(frame) - offset;
            fmo_pcm_result_t result = fmo_pcm_feed(&pcm, 2, true, sizeof(frame),
                                                  offset, frame + offset, chunk, now);
            offset += chunk;
            assert(result == (offset == sizeof(frame) ? FMO_PCM_COMPLETE : FMO_PCM_MORE));
            next += pcm.dropped_samples - dropped;
            dropped = pcm.dropped_samples;
            size_t count = fmo_pcm_read(&pcm, output, FMO_PCM_CHUNK_SAMPLES, now);
            for (size_t i = 0; i < count; ++i)
                assert(output[i] == (int16_t)((int)((next + i) % 2560) - 1280));
            for (size_t i = count; i < FMO_PCM_CHUNK_SAMPLES; ++i) assert(output[i] == 0);
            next += count;
        }
    }
    while (pcm.count) {
        size_t count = fmo_pcm_read(&pcm, output, FMO_PCM_CHUNK_SAMPLES, 100000);
        assert(count);
        for (size_t i = 0; i < count; ++i)
            assert(output[i] == (int16_t)((int)((next + i) % 2560) - 1280));
        for (size_t i = count; i < FMO_PCM_CHUNK_SAMPLES; ++i) assert(output[i] == 0);
        next += count;
    }
    assert(next == (6 + 80) * 2560);
}

int main(void)
{
    assert(fmo_pcm_message_reserve(2,true,5120,0)==2560);
    assert(fmo_pcm_message_reserve(2,false,1436,0)==FMO_PCM_MESSAGE_BYTES/2);
    assert(fmo_pcm_message_reserve(0,true,1436,0)==0);
    assert(fmo_pcm_message_reserve(2,true,5120,1024)==0);
    assert(fmo_pcm_message_reserve(9,true,0,0)==0);
    assert(fmo_pcm_message_reserve(2,true,FMO_PCM_MESSAGE_BYTES+1,0)==0);
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
    /* Consecutive 320 ms frames must survive LAN bursts without losing words. */
    fmo_pcm_reset(&pcm);
    for (unsigned i=0;i<3;++i) feed_frame(4000+i);
    assert(pcm.count==3*2560 && !pcm.dropped_samples);
    for (size_t block=0;block<48;++block) {
        assert(fmo_pcm_read(&pcm,output,160,4010+block*20)==160);
        for(size_t i=0;i<160;++i)
            assert(output[i]==(int16_t)((int)((block*160+i)%2560)-1280));
    }
    /* Even a large partial message stays silent until its final fragment. */
    fmo_pcm_reset(&pcm);
    assert(fmo_pcm_feed(&pcm,2,true,5120,0,frame,1024,5000)==FMO_PCM_MORE);
    assert(fmo_pcm_read(&pcm,output,160,5000)==0);
    assert(fmo_pcm_read(&pcm,output,160,5030)==0);
    assert(fmo_pcm_feed(&pcm,2,true,5120,1024,frame+1024,1024,5030)==FMO_PCM_MORE);
    assert(pcm.count==0 && pcm.pending_count==1024);
    for(uint64_t t=5030;t<8530;t+=20)
        assert(fmo_pcm_read(&pcm,output,160,t)==0);
    /* A delayed remainder keeps the frame aligned and preserves queued speech. */
    for(size_t offset=2048;offset<sizeof(frame);offset+=1024)
        assert(fmo_pcm_feed(&pcm,2,true,sizeof(frame),offset,frame+offset,1024,8530)
               ==(offset+1024==sizeof(frame) ? FMO_PCM_COMPLETE : FMO_PCM_MORE));
    assert(pcm.count==2560 && !pcm.pending_count && !pcm.active && !pcm.dropped_samples);
    assert(fmo_pcm_read(&pcm,output,160,8530)==160 && output[0]==-1280);
    for(size_t block=1;block<16;++block) {
        assert(fmo_pcm_read(&pcm,output,160,8530+block*20)==160);
        for(size_t i=0;i<160;++i)assert(output[i]==(int16_t)((int)(block*160+i)-1280));
    }
    /* A committed message may drain while the next message is still arriving. */
    fmo_pcm_reset(&pcm);feed_frame(9000);
    assert(fmo_pcm_feed(&pcm,2,true,5120,0,frame,1024,9001)==FMO_PCM_MORE);
    assert(pcm.count==2560 && pcm.pending_count==512);
    for(size_t block=0;block<16;++block) {
        assert(fmo_pcm_read(&pcm,output,160,9001+block*20)==160);
        for(size_t i=0;i<160;++i)assert(output[i]==(int16_t)((int)(block*160+i)-1280));
    }
    assert(fmo_pcm_read(&pcm,output,160,9400)==0 && pcm.pending_count==512);
    for(size_t off=1024;off<sizeof(frame);off+=1024)
        fmo_pcm_feed(&pcm,2,true,5120,off,frame+off,1024,9500);
    assert(pcm.count==2560 && !pcm.pending_count);
    assert(fmo_pcm_read(&pcm,output,160,9500)==160 && output[0]==-1280);
    /* Long speech is many bounded messages, not a one-second speech limit. */
    fmo_pcm_reset(&pcm);
    for(unsigned message=0;message<100;++message) {
        uint64_t t=10000+message*320;
        feed_frame(t);
        for(size_t block=0;block<16;++block) {
            assert(fmo_pcm_read(&pcm,output,160,t+block*20)==160);
            for(size_t i=0;i<160;++i)assert(output[i]==(int16_t)((int)(block*160+i)-1280));
        }
        assert(!pcm.count && !pcm.pending_count && !pcm.dropped_samples);
    }
    /* Every accepted one-second message fits even before playback begins. */
    static uint8_t full_second[FMO_PCM_MESSAGE_BYTES];
    for (size_t i=0;i<sizeof(full_second);i+=2) {
        full_second[i]=0x34;full_second[i+1]=0x12;
    }
    fmo_pcm_reset(&pcm);
    assert(fmo_pcm_feed(&pcm,2,true,sizeof(full_second),0,full_second,
                        sizeof(full_second),5000)==FMO_PCM_COMPLETE);
    assert(pcm.count==FMO_PCM_RATE);
    for(unsigned i=0;i<50;++i) {
        assert(fmo_pcm_read(&pcm,output,160,5000+i*20)==160);
        for(unsigned j=0;j<160;++j)assert(output[j]==0x1234);
    }
    /* Slow playback remains bounded and drops complete oldest samples. */
    fmo_pcm_reset(&pcm);
    for(unsigned i=0;i<6;++i)feed_frame(6000+i);
    assert(pcm.count==FMO_PCM_CAPACITY && pcm.dropped_samples==3072);
    assert(fmo_pcm_read(&pcm,output,160,6010)==160);
    assert(output[0]==-768); /* Dropped 3072 oldest samples across frame boundaries. */
    fmo_pcm_reset(&pcm);
    assert(fmo_pcm_read(&pcm,output,160,6020)==0 && output[0]==0);
    verify_ring_stream();
    /* Reset must hide both committed and pending backing samples, even when
     * the backing array is deliberately left dirty by a metadata-only reset. */
    feed_frame(110000);
    assert(fmo_pcm_feed(&pcm, 2, true, 5120, 0, frame, 1, 110001) == FMO_PCM_MORE);
    fmo_pcm_reset(&pcm);
    assert(!pcm.count && !pcm.pending_count && !pcm.partial_sample && !pcm.active);
    memset(output, 0x5a, sizeof(output));
    assert(fmo_pcm_read(&pcm, output, FMO_PCM_CHUNK_SAMPLES, 110100) == 0);
    for (size_t i = 0; i < FMO_PCM_CHUNK_SAMPLES; ++i) assert(output[i] == 0);
    assert(fmo_pcm_feed(&pcm, 2, true, 2, 0, signed_samples, 2, 110200) == FMO_PCM_COMPLETE);
    assert(fmo_pcm_read(&pcm, output, FMO_PCM_CHUNK_SAMPLES, 110260) == 1);
    assert(output[0] == INT16_MIN);
    for (size_t i = 1; i < FMO_PCM_CHUNK_SAMPLES; ++i) assert(output[i] == 0);
    puts("FMO PCM: PASS (live frame size, chunk splits, signed PCM, burst continuity, full-second message, buffering, overflow, reset)");
}
