// SPDX-License-Identifier: GPL-2.0-only
// Userspace adaptation modified by Dylan Ross, 2026-11-15; original notices retained below.
// SPDX-License-Identifier: GPL-2.0-only
#ifndef BDC_HDTVMATE_H
#define BDC_HDTVMATE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BDCMate BDCMate;
typedef struct {
 int locked,standard,snr_valid,rssi_valid;
 double snr_db,rssi_dbm;
 uint32_t firmware;
 int chip_id;
 int plp_count;
 int plp_ids[4],plp_locked[4],plp_modulation[4],plp_coderate[4],plp_fec[4];
} BDCMateStatus;
typedef struct {
 int sync_state,bootstrap_valid,bootstrap_bandwidth,bootstrap_bandwidth_diff,emergency_wakeup;
 int basic_valid,detail_valid,detail_size,detail_read_status;
 uint8_t basic_registers[29];
 int preamble_parameters_valid;
 uint8_t preamble_registers[3]; // Sony 90:59..5B, only at demodulator lock.

} BDCMateSignaling;
int bdc_mate_signaling(BDCMate*,BDCMateSignaling*,uint8_t *detail,int capacity);
char *bdc_mate_discover(void); // malloc JSON, caller frees
BDCMate *bdc_mate_open(const char *serial,const char *firmware,char *error,int error_size);
int bdc_mate_tune(BDCMate*,int frequency_hz,int standard,const int *plps,int plp_count);
int bdc_mate_select_plps(BDCMate*,const int *plps,int plp_count);
int bdc_mate_status(BDCMate*,BDCMateStatus*);
int bdc_mate_read(BDCMate*,uint8_t *bytes,int capacity,int timeout_ms);
void bdc_mate_close(BDCMate*);
#ifdef __cplusplus
}
#endif
#endif
