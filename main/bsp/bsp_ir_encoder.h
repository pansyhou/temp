#ifndef BSP_IR_H
#define BSP_IR_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/rmt_encoder.h"


typedef struct {
    uint32_t resolution; // RMT 时钟分辨率，例如 1MHz
} ir_kelon_encoder_config_t;

//函数声明
esp_err_t rmt_new_ir_kelon_encoder(const ir_kelon_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);
static esp_err_t rmt_del_ir_kelon_encoder(rmt_encoder_t *encoder);
static esp_err_t rmt_ir_kelon_encoder_reset(rmt_encoder_t *encoder);
static esp_err_t rmt_ir_kelon_encoder_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *priming_buf, size_t priming_buf_size, size_t *encoded_symbols, rmt_encode_state_t *ret_state);
void bsp_rmt_init();
void bsp_ir_send(uint64_t data);
#endif