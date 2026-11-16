// SPDX-License-Identifier: GPL-2.0-only
// Userspace adaptation modified by Dylan Ross, 2026-11-15; original notices retained below.
// SPDX-License-Identifier: GPL-2.0-only
// Modified 2026-11-16: portable POSIX monotonic clock.
// Minimal userspace adapters for the cxd28xx hardware-control implementation.
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#ifdef BDC_IPAD_TV_NATIVE
#include "../iPadTV/libusb.h"
#else
#include "libusb.h"
#endif
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define min_t(t,a,b) ((t)min(a,b))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define U8_MAX 255
#define MHz 1000000
#define kHz 1000
#define HZ 1000
// POSIX monotonic time on both Darwin and Linux; never use wall-clock timeouts.
static inline u32 bdc_monotonic_ms(void) {
 struct timespec t;
 if(clock_gettime(CLOCK_MONOTONIC,&t)!=0)abort();
 return (u32)((uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000);
}
#define jiffies bdc_monotonic_ms()
#define GFP_KERNEL 0
#define kzalloc(n,f) calloc(1,n)
#define kfree free
#define KBUILD_MODNAME "hdtvmate"
#define IS_REACHABLE(x) 1
#define msleep(n) usleep((n)*1000)
#define usleep_range(a,b) usleep(a)
#define dev_info(d,...) fprintf(stderr,__VA_ARGS__)
#define dev_warn(d,...) fprintf(stderr,__VA_ARGS__)
#define dev_err(d,...) fprintf(stderr,__VA_ARGS__)
#define dev_dbg(d,...) ((void)0)
struct mutex { pthread_mutex_t value; };
static void mutex_init(struct mutex *m) { pthread_mutex_init(&m->value,NULL); }
static void mutex_lock(struct mutex *m) { pthread_mutex_lock(&m->value); }
static void mutex_unlock(struct mutex *m) { pthread_mutex_unlock(&m->value); }
struct list_head { struct list_head *next; };
#define LIST_HEAD(n) struct list_head n={0}
#define list_for_each_entry(p,h,m) for(p=NULL;p!=NULL;)
#define list_add(a,b) ((void)0)
#define list_del(a) ((void)0)
struct device { int dummy; };
struct i2c_adapter { struct device dev; void *data; };
struct i2c_msg { u16 addr,flags,len; u8 *buf; };
#define I2C_M_RD 1
#define i2c_get_adapdata(a) ((a)->data)
static int i2c_transfer(struct i2c_adapter*,struct i2c_msg*,int);
struct net_device { int dummy; };
#define netif_carrier_on(a) ((void)0)
#define netif_carrier_off(a) ((void)0)
enum fe_delivery_system { SYS_DVBT=1,SYS_DVBT2,SYS_DVBC_ANNEX_A,SYS_DVBC_ANNEX_B,SYS_DVBC_ANNEX_C,SYS_ISDBT,SYS_DVBS2,SYS_ATSC,SYS_ATSC3=35 };
enum fe_status { FE_HAS_SIGNAL=1,FE_HAS_CARRIER=2,FE_HAS_VITERBI=4,FE_HAS_SYNC=8,FE_HAS_LOCK=16 };
enum dvbfe_algo { DVBFE_ALGO_HW };
enum { FE_SCALE_NOT_AVAILABLE,FE_SCALE_DECIBEL,FE_SCALE_RELATIVE,FE_SCALE_COUNTER };
enum { QPSK,QAM_16,QAM_32,QAM_64,QAM_128,QAM_256,FEC_1_2,FEC_1_3,FEC_2_3,FEC_2_5,FEC_3_4,FEC_3_5,FEC_4_5,FEC_5_6,FEC_7_8,DTV_DELIVERY_SYSTEM };
#define NO_STREAM_ID_FILTER UINT32_MAX
struct dtv_stats { int scale; union { s64 svalue; u64 uvalue; }; };
struct dtv_fe_stats { int len; struct dtv_stats stat[4]; };
struct dtv_frontend_properties { enum fe_delivery_system delivery_system; u32 frequency,bandwidth_hz,stream_id,symbol_rate; int modulation,code_rate_HP,fec_inner,inversion; struct dtv_fe_stats strength,cnr,post_bit_error,post_bit_count; };
struct dvb_frontend_ops { struct { u32 frequency_min_hz,frequency_max_hz,frequency_stepsize_hz,symbol_rate_min,symbol_rate_max; } info; };
struct dvb_frontend { struct dvb_frontend_ops ops; void *demodulator_priv; struct dtv_frontend_properties dtv_property_cache; };
struct usb_device { libusb_device_handle *handle; };
struct usb_interface { struct device dev; };
#define usb_sndbulkpipe(d,e) (e)
#define usb_rcvbulkpipe(d,e) (e)
static int usb_bulk_msg(struct usb_device *d,int ep,void *buf,int len,int *actual,int timeout) { int r=libusb_bulk_transfer(d->handle,ep,buf,len,actual,timeout);return r<0?r:0; }
struct firmware { size_t size; const u8 *data; };
static const char *firmware_path;
static int request_firmware(const struct firmware **out,const char *name,struct device *d) {
 FILE *f=firmware_path?fopen(firmware_path,"rb"):NULL;if(!f)return -ENOENT;
 fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<=0||n>1048576){fclose(f);return -EINVAL;}
 struct firmware *fw=calloc(1,sizeof(*fw));u8 *b=malloc(n);
 if(!fw||!b){free(fw);free(b);fclose(f);return -ENOMEM;}
 if(fread(b,1,n,f)!=(size_t)n){free(fw);free(b);fclose(f);return -EIO;}
 fclose(f);fw->size=n;fw->data=b;*out=fw;return 0;
}
static void release_firmware(const struct firmware *fw){free((void*)fw->data);free((void*)fw);}
#define ENOTSUPP ENOTSUP
#define VSB_8 100
#define printk(...) fprintf(stderr,__VA_ARGS__)
