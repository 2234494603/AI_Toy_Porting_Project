/**
 * @file hfp_hf_demo.h
 *
 */

#ifndef HFP_HF_DEMO_H
#define HFP_HF_DEMO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*hfp_hf_audio_state_callback_t)(bool active, void *user_data);

int hfp_hf_demo_init(uint8_t msbc_supported);
void hfp_hf_demo_register_audio_state_callback(
    hfp_hf_audio_state_callback_t callback,
    void *user_data);
void hfp_demo_vr(uint8_t enable);
void hfp_demo_dial(uint8_t enable, uint8_t *num);
void hfp_demo_answer(uint8_t accept);
void hfp_demo_cust_cmd(uint8_t *cmd);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*HFP_HF_DEMO_H*/
