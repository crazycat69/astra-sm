#ifndef _TS_SYNC_
#define _TS_SYNC_ 1

typedef uint8_t ts_packet_t[TS_PACKET_SIZE];

typedef struct mpegts_sync_t mpegts_sync_t;

typedef void (*sync_callback_t)(void *);

mpegts_sync_t *mpegts_sync_init(void);
void mpegts_sync_destroy(mpegts_sync_t *sync);
void mpegts_sync_loop(void *arg);
bool mpegts_sync_push(mpegts_sync_t *sync, void *buf, size_t count);
void mpegts_sync_set_arg(mpegts_sync_t *sync, void *arg);
void mpegts_sync_set_on_read(mpegts_sync_t *sync, sync_callback_t on_read);
void mpegts_sync_set_on_write(mpegts_sync_t *sync, ts_callback_t on_write);
size_t mpegts_sync_space(mpegts_sync_t *sync);

#endif /* _TS_SYNC_ */
