// SPDX-License-Identifier: GPL-2.0-only
// Userspace adaptation modified by Dylan Ross, 2026-11-15; original notices retained below.
// SPDX-License-Identifier: GPL-2.0-only
// Modified 2026-11-16: Linux automatic kernel-driver detach/reattach and claim tracking.
// macOS/Linux userspace port of koreapyj/cxd28xx. See README for provenance.
#include "compat.h"
#include <math.h>
#include "hdtvmate.h"
#include "cxd2878.h"
#include "cxd2878_priv.h"
struct it930x_dev;
struct it930x_i2c_ctx { struct it930x_dev *dev; u8 bus_num; };
struct it930x_dev {
 struct usb_device *udev; struct usb_interface *intf;
 struct mutex cmd_mutex,io_lock; u8 *tx_bulk_buf,*rx_bulk_buf,seq;
 u32 fw_version; const struct it930x_board_cfg *board;
};
#include "bridge.inc"
static int i2c_transfer(struct i2c_adapter *a,struct i2c_msg *m,int n){return it930x_i2c_xfer(a,m,n);}
#include "demod.inc"
struct BDCMate {
 libusb_context *context; libusb_device_handle *handle;
 struct usb_device usb; struct usb_interface intf;
 struct it930x_dev bridge; struct it930x_i2c_ctx i2cctx;
 struct i2c_adapter i2c; struct cxd2878_config config;
 struct dvb_frontend *frontend; int standard; int interface_claimed;
};
static const struct it930x_board_cfg mate_board={
 .name="GTMedia HDTV Mate",.fw_file="dvb-usb-it9303-01.fw",.num_frontends=1,
 .fe={{.ts_port=0,.i2c_bus=3,.demod_addr=0x6c,.tuner_addr=0x60}},
 .gpio_reset=2,.gpio_always_hi=14,.i2c_notify=0x38,.tuner_xtal=SONY_ASCOT3_XTAL_24000KHz
};
static void serial_string(libusb_device_handle *h,u8 idx,char *out,int cap) {
 int n=idx?libusb_get_string_descriptor_ascii(h,idx,(u8*)out,cap-1):0;
 if(n<0)n=0;out[n]=0;
 for(int i=0;i<n;i++)if((unsigned char)out[i]<32||out[i]=='"'||out[i]=='\\')out[i]='_';
}
char *bdc_mate_discover(void){
 libusb_context *ctx=NULL; if(libusb_init(&ctx))return strdup("[]");
 libusb_device **list;ssize_t n=libusb_get_device_list(ctx,&list);
 char *json=calloc(1,8192);size_t used=0;used+=snprintf(json,8192,"[");
 for(ssize_t i=0;i<n;i++){
  struct libusb_device_descriptor d;if(libusb_get_device_descriptor(list[i],&d)||d.idVendor!=0x23e2||d.idProduct!=0x2b02)continue;
  libusb_device_handle *h=NULL;char s[128]={0};if(!libusb_open(list[i],&h)){serial_string(h,d.iSerialNumber,s,sizeof(s));libusb_close(h);}
  if(!s[0])snprintf(s,sizeof(s),"usb-%u-%u",libusb_get_bus_number(list[i]),libusb_get_device_address(list[i]));
  if(used+300<8192)used+=snprintf(json+used,8192-used,"%s{\"serial\":\"%s\",\"vendor\":9186,\"product\":11010}",used>1?",":"",s);
 }
 snprintf(json+used,8192-used,"]");if(n>=0)libusb_free_device_list(list,1);libusb_exit(ctx);return json;
}
BDCMate *bdc_mate_open(const char *serial,const char *firmware,char *error,int cap){
 BDCMate *m=calloc(1,sizeof(*m));if(!m)return NULL;int r=libusb_init(&m->context);
 if(r)goto fail;
 libusb_device **list;ssize_t n=libusb_get_device_list(m->context,&list);
 if(n<0){r=(int)n;goto fail;}
 int open_error=-ENODEV;
 for(ssize_t i=0;i<n;i++){
  struct libusb_device_descriptor d;if(libusb_get_device_descriptor(list[i],&d)||d.idVendor!=0x23e2||d.idProduct!=0x2b02)continue;
  libusb_device_handle *h=NULL;r=libusb_open(list[i],&h);if(r){open_error=r;continue;}
  char s[128]={0};serial_string(h,d.iSerialNumber,s,sizeof(s));
  if(!s[0])snprintf(s,sizeof(s),"usb-%u-%u",libusb_get_bus_number(list[i]),libusb_get_device_address(list[i]));
  if(!serial||!strcmp(serial,s)){m->handle=h;break;}libusb_close(h);
 }
 if(n>=0)libusb_free_device_list(list,1);
 if(!m->handle){r=open_error;goto fail;}
#if defined(__linux__)
 if(libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
  r=libusb_set_auto_detach_kernel_driver(m->handle,1);if(r)goto fail;
 }
#endif
 r=libusb_claim_interface(m->handle,0);if(r)goto fail;m->interface_claimed=1;
 m->usb.handle=m->handle;m->bridge.udev=&m->usb;m->bridge.intf=&m->intf;m->bridge.board=&mate_board;
 mutex_init(&m->bridge.cmd_mutex);mutex_init(&m->bridge.io_lock);
 m->bridge.tx_bulk_buf=calloc(1,512);m->bridge.rx_bulk_buf=calloc(1,512);
 if(!m->bridge.tx_bulk_buf||!m->bridge.rx_bulk_buf){r=-ENOMEM;goto fail;}
 firmware_path=firmware;
 r=it930x_hw_init(&m->bridge);if(r)goto fail;
 m->i2cctx.dev=&m->bridge;m->i2cctx.bus_num=3;m->i2c.data=&m->i2cctx;
 m->config=(struct cxd2878_config){.addr_slvt=0x6c,.xtal=SONY_DEMOD_XTAL_24000KHz,.tuner_addr=0x60,
  .tuner_xtal=SONY_ASCOT3_XTAL_24000KHz,.ts_clk=1,.ts_clk_mask=1,.lock_flag=0,.atsc3_output=SONY_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS};
 m->frontend=cxd2878_attach(&m->config,&m->i2c);if(!m->frontend){r=-ENODEV;goto fail;}
 r=cxd2878_init(m->frontend);if(r)goto fail;
 r=it930x_stage_ts_port_cfg(&m->bridge);if(r)goto fail;
 return m;
fail:
 snprintf(error,cap,"HDTV Mate initialization failed (code %d).%s",r,
 r==-ENOENT?" Bridge firmware file is missing.":r==LIBUSB_ERROR_ACCESS?" USB access denied; on Linux install the supplied udev rule.":r==LIBUSB_ERROR_BUSY?" USB interface is busy; stop other receiver applications.":"");
 bdc_mate_close(m);return NULL;
}
int bdc_mate_select_plps(BDCMate *m,const int *plps,int count){
 if(!m||m->standard!=3||!plps||count<1||count>4)return -EINVAL;
 u8 ids[4]={0};for(int i=0;i<count;i++){if(plps[i]<0||plps[i]>63)return -EINVAL;for(int j=0;j<i;j++)if(plps[i]==plps[j])return -EINVAL;ids[i]=0x80|plps[i];}
 struct cxd2878_dev *d=m->frontend->demodulator_priv;
 int r=cxd2878_wr(d,d->slvt,0,0x93);if(!r)r=cxd2878_wr(d,d->slvt,0x85,0);
 if(!r)r=cxd2878_wrm(d,d->slvt,0x80,ids,4);if(!r)r=cxd2878_wr(d,d->slvt,0x9c,1);
 return r;
}
int bdc_mate_tune(BDCMate *m,int hz,int standard,const int *plps,int count){
 if(!m||(standard!=1&&standard!=3)||hz<54000000||hz>806000000||count<0||count>4||(count&&!plps))return -EINVAL;
 for(int i=0;i<count;i++){if(plps[i]<0||plps[i]>63)return -EINVAL;for(int j=0;j<i;j++)if(plps[i]==plps[j])return -EINVAL;}
 struct dvb_frontend *fe=m->frontend;
 fe->dtv_property_cache.frequency=hz;fe->dtv_property_cache.bandwidth_hz=6000000;
 fe->dtv_property_cache.delivery_system=standard==3?SYS_ATSC3:SYS_ATSC;
 fe->dtv_property_cache.modulation=standard==1?VSB_8:QPSK;
 fe->dtv_property_cache.stream_id=count?(u32)plps[0]:NO_STREAM_ID_FILTER;
 int r=cxd2878_set_frontend(fe);if(r)return r;m->standard=standard;
 if(standard==3&&count>1)r=bdc_mate_select_plps(m,plps,count);
 return r;
}
static int mate_bank_read(struct cxd2878_dev*,u8,u8,u8*,u8);
int bdc_mate_status(BDCMate *m,BDCMateStatus *s){
 if(!m||!s)return -EINVAL;memset(s,0,sizeof(*s));s->standard=m->standard;
 s->firmware=m->bridge.fw_version;s->chip_id=((struct cxd2878_dev*)m->frontend->demodulator_priv)->chipid;
 if(!m->standard)return 0;
 enum fe_status status=0;int r=cxd2878_read_status(m->frontend,&status);if(r)return r;
 s->locked=(status&FE_HAS_LOCK)!=0;struct dtv_frontend_properties *p=&m->frontend->dtv_property_cache;
 s->rssi_valid=p->strength.stat[0].scale==FE_SCALE_DECIBEL;s->rssi_dbm=p->strength.stat[0].svalue/1000.0;
 s->snr_valid=s->locked&&p->cnr.stat[0].scale==FE_SCALE_DECIBEL;s->snr_db=p->cnr.stat[0].svalue/1000.0;
 if(m->standard==3){
  struct cxd2878_dev *d=m->frontend->demodulator_priv;u8 ids[4]={0},locks=0,mod[2]={0},cr[2]={0},fec[2]={0};
  r=cxd2878_wr(d,d->slvt,0,0x93);r|=cxd2878_rdm(d,d->slvt,0x80,ids,4);
  r|=cxd2878_rdm(d,d->slvt,0x92,mod,2);r|=cxd2878_rdm(d,d->slvt,0x94,cr,2);r|=cxd2878_rdm(d,d->slvt,0x90,fec,2);
  r|=cxd2878_wr(d,d->slvt,0,0x95);r|=cxd2878_rdm(d,d->slvt,0x40,&locks,1);
  if(r)return r;
  // OFDM SNR is valid at demodulator lock, independently of ALP payload lock.
  u8 state=0,noise[3]={0},nsbf=0;
  r=cxd2878_wr(d,d->slvt,1,1);
  if(!r)r=mate_bank_read(d,0x90,0x10,&state,1);
  if(!r)r=mate_bank_read(d,0x90,0x28,noise,3);
  if(!r)r=mate_bank_read(d,0x90,0x58,&nsbf,1);
  int uf=cxd2878_wr(d,d->slvt,1,0);if(r||uf)return r?r:uf;
  u32 v=((u32)noise[0]<<16)|((u32)noise[1]<<8)|noise[2];
  s->snr_valid=((state&7)==6)&&!(state&0x10)&&v>0;
  if(s->snr_valid){double limit=nsbf?704089:419404,scale=nsbf?971204:519913;
   double x=fmin(v,limit);s->snr_db=10*log10(x/(scale-x))+(nsbf?35.80:33.80);}
  for(int i=0;i<4;i++)if(ids[i]&0x80){int j=s->plp_count++;s->plp_ids[j]=ids[i]&63;s->plp_locked[j]=(locks>>i)&1;
   s->plp_modulation[j]=(mod[i/2]>>(i%2?0:4))&15;s->plp_coderate[j]=(cr[i/2]>>(i%2?0:4))&15;s->plp_fec[j]=(fec[i/2]>>(i%2?0:4))&15;}
 }
 return 0;
}
// Register maps verified against Sony CXD6801 monitor functions. Bootstrap and
// L1B readiness do not depend on ALP lock. Never wait on the UI thread for L1D.
static int mate_bank_read(struct cxd2878_dev *d,u8 bank,u8 reg,u8 *b,u8 n){
 int r=cxd2878_wr(d,d->slvt,0,bank);if(r)return r;
 // Generic IT9303 firmware's staged long-read buffer is not reliable here.
 // Read consecutive monitor registers with bounded ordinary I2C transactions.
 for(int off=0;off<n;off+=32){r=cxd2878_rdm(d,d->slvt,reg+off,b+off,min(32,n-off));if(r)return r;}return 0;
}
int bdc_mate_signaling(BDCMate *m,BDCMateSignaling *s,u8 *detail,int capacity){
 if(!m||!s||m->standard!=3)return -EINVAL;
 memset(s,0,sizeof(*s));struct cxd2878_dev *d=m->frontend->demodulator_priv;
 u8 bs=0,flags=0,state=0,info[3]={0};int r=cxd2878_wr(d,d->slvt,1,1);
 if(!r)r=mate_bank_read(d,0x90,0x10,&state,1);
 if(!r)r=mate_bank_read(d,0x90,0xe6,&bs,1);
 if(!r&&(bs&1))r=mate_bank_read(d,0x90,0xe0,info,3);
 if(!r)r=mate_bank_read(d,0x93,0x10,&flags,1);
 if(!r&&(flags&1))r=mate_bank_read(d,0x93,0x60,s->basic_registers,29);
 // Sony's separate demodulator-parameter monitor requires sync state 6.
 // These are decoded operating parameters, NOT a raw bootstrap bit word.
 if(!r&&(bs&1)&&(state&7)==6&&!(state&0x10)){
  int pr=mate_bank_read(d,0x90,0x59,s->preamble_registers,3);
  s->preamble_parameters_valid=pr==0;
 }

 int unfreeze=cxd2878_wr(d,d->slvt,1,0);if(r||unfreeze)return r?r:unfreeze;
 s->sync_state=state&7;s->bootstrap_valid=bs&1;s->bootstrap_bandwidth=info[1]&3;
 s->bootstrap_bandwidth_diff=info[0]&1;s->emergency_wakeup=info[2]&3;
 s->basic_valid=flags&1;s->detail_valid=(flags>>1)&1;
 if(!s->basic_valid||!s->detail_valid||!detail||capacity<1)return 0;
 u8 ready[2]={0};r=mate_bank_read(d,0x93,0xe1,ready,1);if(r)return r;
 if(!(ready[0]&0x80))return 0;
 r=cxd2878_wr(d,d->slvt,0xe0,1);if(r)return r;
 r=mate_bank_read(d,0x93,0xe1,ready,2);
 if(!r&&(ready[0]&0x20)&&!(ready[0]&0x40)){
  int n=((ready[0]&31)<<8)|ready[1];
  if(n>=4&&n<=capacity){
   // Snapshot L1B under the same L1 RAM freeze to pair the correct generations.
   r=mate_bank_read(d,0x93,0x60,s->basic_registers,29);
   for(int off=0;!r&&off<n;off+=240){
    r=cxd2878_wr(d,d->slvt,0,0x93);if(!r)r=cxd2878_wr(d,d->slvt,0xe3,off/240);
    if(!r)r=mate_bank_read(d,0x92,0x10,detail+off,min(240,n-off));
   }
   if(!r)s->detail_size=n;
  }
 }
 // Unfreeze L1 RAM even on a failed transfer or a not-ready snapshot.
 int release=cxd2878_wr(d,d->slvt,0,0x93);release|=cxd2878_wr(d,d->slvt,0xe0,0);
 s->detail_read_status=r?r:release;return 0;
}
int bdc_mate_read(BDCMate *m,u8 *b,int cap,int timeout){
 if(!m)return -EINVAL;int actual=0;int r=libusb_bulk_transfer(m->handle,0x84,b,cap,&actual,timeout);
 if(r==LIBUSB_ERROR_TIMEOUT)return actual;return r?r:actual;
}
void bdc_mate_close(BDCMate *m){if(!m)return;
 if(m->frontend)cxd2878_release(m->frontend);
 free(m->bridge.tx_bulk_buf);free(m->bridge.rx_bulk_buf);
 if(m->handle){if(m->interface_claimed)libusb_release_interface(m->handle,0);libusb_close(m->handle);}
 if(m->context)libusb_exit(m->context);free(m);
}
#ifdef BDC_MATE_PROBE
int main(int argc,char **argv){
 if(argc<2){char *s=bdc_mate_discover();puts(s);free(s);return 0;}
 char e[512];BDCMate *m=bdc_mate_open(argv[1],argc>2?argv[2]:NULL,e,sizeof(e));if(!m){fprintf(stderr,"%s\n",e);return 1;}
 int hz=argc>3?atoi(argv[3]):587000000;int standard=argc>4?atoi(argv[4]):1;
 int r=bdc_mate_tune(m,hz,standard,NULL,0);fprintf(stderr,"Tune result %d\n",r);
 u8 buf[188*348];unsigned long long bytes=0;FILE *capture=argc>5?fopen(argv[5],"wb"):NULL;
 for(int i=0;i<600;i++) {int n=bdc_mate_read(m,buf,sizeof(buf),100);if(n<0){fprintf(stderr,"Read %d\n",n);break;}if(n>0){bytes+=n;if(capture)fwrite(buf,1,n,capture);}
  if(i%10==0){BDCMateStatus s;int err=bdc_mate_status(m,&s);printf("status=%d locked=%d rssi=%.3f snr=%s%.3f bytes=%llu chip=%x fw=%08x\n",err,s.locked,s.rssi_dbm,s.snr_valid?"":"unavailable ",s.snr_db,bytes,s.chip_id,s.firmware);fflush(stdout);
 if(standard==3){BDCMateSignaling sig;u8 detail[8192];int x=bdc_mate_signaling(m,&sig,detail,sizeof(detail));
 printf("signaling err=%d sync=%d bs=%d bw=%d ea=%d L1B=%d L1D=%d detail=%d read=%d\n",x,sig.sync_state,sig.bootstrap_valid,sig.bootstrap_bandwidth,sig.emergency_wakeup,sig.basic_valid,sig.detail_valid,sig.detail_size,sig.detail_read_status);
 if(sig.basic_valid){FILE *f=fopen("/tmp/bdc-mate-l1b.bin","wb");if(f){fwrite(sig.basic_registers,1,29,f);fclose(f);}}
 if(sig.detail_size>0){FILE *f=fopen("/tmp/bdc-mate-l1d.bin","wb");if(f){fwrite(detail,1,sig.detail_size,f);fclose(f);}}}}}
 if(capture)fclose(capture);bdc_mate_close(m);return r?1:0;
}
#endif
