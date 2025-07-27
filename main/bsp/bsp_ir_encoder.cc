#include "bsp_ir_encoder.h"
#include "esp_check.h"
#include <ir_kelon.h>
#include <functional>

#include <stdlib.h> // For free()
#include "esp_err.h"
#include "esp_log.h"

#include "driver/rmt_tx.h"

#define IR_TX_GPIO_NUM GPIO_NUM_37

static const char *TAG = "kelon_encoder"; // 日志标签
#define kKelonHdrMark  9000ULL
#define kKelonHdrSpace  4600ULL
#define kKelonBitMark  560ULL
#define kKelonOneSpace  1680ULL
#define kKelonZeroSpace  600ULL

#define kDefaultMessageGap 500000 // 假设一个默认消息间隔，例如 500ms，（反正没用
const uint32_t kKelonGap = 2 * kDefaultMessageGap; // 转换为微秒

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    rmt_encoder_t *bytes_encoder;
    rmt_symbol_word_t kelon_leading_symbol;
    rmt_symbol_word_t kelon_ending_symbol;  
    // rmt_symbol_word_t kelon_delay_symbol;
    int state;
} rmt_ir_kelon_encoder_t;


rmt_encoder_handle_t ir_encoder = NULL;
rmt_channel_handle_t tx_channel = NULL;
rmt_transmit_config_t transmit_config = {
    .loop_count = 0, // no loop
};
//rmt 初始化
void bsp_rmt_init()
{
ESP_LOGI(TAG, "create RMT TX channel");
    rmt_tx_channel_config_t tx_channel_cfg = {
        .gpio_num = IR_TX_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
        .trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
        // .flags = { .with_dma = true },
    };
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));

    ESP_LOGI(TAG, "modulate carrier to TX channel");
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000, // 38KHz
        .duty_cycle = 0.33,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));



    ESP_LOGI(TAG, "install IR kelon encoder");
    ir_kelon_encoder_config_t kelon_encoder_cfg = {
        .resolution = 1000000,
    };


    ESP_ERROR_CHECK(rmt_new_ir_kelon_encoder(&kelon_encoder_cfg, &ir_encoder));

    ESP_LOGI(TAG, "enable RMT TX and RX channels");
    ESP_ERROR_CHECK(rmt_enable(tx_channel));


}


static size_t rmt_encode_ir_kelon(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                        const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_ir_kelon_encoder_t *ir_encoder = __containerof(encoder, rmt_ir_kelon_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ir_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = ir_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    // convert user data (ir structure) into 48bits frame

    uint8_t frame[kKelonFrameSize];
    make_kelon_frame(primary_data, frame, sizeof(frame));
    //引导码 (leading) -> 48 位数据 (Data Bits) -> 结束码 (ending) -> 消息间间隔 (Gap)
    switch (ir_encoder->state) {
        case 0://leading with copy_encoder
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ir_encoder->kelon_leading_symbol,
                                                sizeof(rmt_symbol_word_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                ir_encoder->state = 1; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out; // yield if there's no free space for encoding artifacts
            }
        case 1: //encode with bytes_encoder
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, &frame, sizeof(frame), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                ir_encoder->state = 2; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out; // yield if there's no free space for encoding artifacts
            }
        // fall-through to ending with copy_encoder
        case 2:
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ir_encoder->kelon_ending_symbol,
                                                    sizeof(rmt_symbol_word_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_COMPLETE);
                ir_encoder->state = RMT_ENCODING_RESET; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out; // yield if there's no free space for encoding artifacts
            }
        }
out:
    *ret_state = state;
    return encoded_symbols;
}


static esp_err_t rmt_del_ir_kelon_encoder(rmt_encoder_t *encoder)
{
    rmt_ir_kelon_encoder_t *kelon_encoder = __containerof(encoder, rmt_ir_kelon_encoder_t, base);
    rmt_del_encoder(kelon_encoder->copy_encoder);
    rmt_del_encoder(kelon_encoder->bytes_encoder);
    free(kelon_encoder);
    return ESP_OK;
}

static esp_err_t rmt_ir_kelon_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_ir_kelon_encoder_t *kelon_encoder = __containerof(encoder, rmt_ir_kelon_encoder_t, base);
    rmt_encoder_reset(kelon_encoder->copy_encoder);
    rmt_encoder_reset(kelon_encoder->bytes_encoder);
    kelon_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}



esp_err_t rmt_new_ir_kelon_encoder(const ir_kelon_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    //按照C++规则，需要初始化所有成员变量
    rmt_ir_kelon_encoder_t *kelon_encoder = NULL;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = (uint16_t)(kKelonBitMark * config->resolution / 1000000),
            .level0 = 1,
            .duration1 = (uint16_t)(kKelonZeroSpace * config->resolution / 1000000),
            .level1 = 0
        },
        .bit1 = {
            .duration0 = (uint16_t)(kKelonBitMark * config->resolution / 1000000),
            .level0 = 1,
            .duration1 = (uint16_t)(kKelonOneSpace * config->resolution / 1000000),
            .level1 = 0,
        },
    };

    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    kelon_encoder = (rmt_ir_kelon_encoder_t*)rmt_alloc_encoder_mem(sizeof(rmt_ir_kelon_encoder_t));
    ESP_GOTO_ON_FALSE(kelon_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for ir kelon encoder"); 

    kelon_encoder->base.encode = rmt_encode_ir_kelon;        // 指向 Kelon 编码函数
    kelon_encoder->base.del = rmt_del_ir_kelon_encoder;      // 指向 Kelon 删除函数
    kelon_encoder->base.reset = rmt_ir_kelon_encoder_reset;  // 指向 Kelon 重置函数





    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &kelon_encoder->copy_encoder), err, TAG, "create copy encoder failed"); 

    // 使用 Kelon 协议的引导码和结束码构建 RMT 符号
    // 计算持续时间时，将微秒 (us) 转换为 RMT 时钟的 ticks
    // 转换公式: ticks = duration_us * resolution / 1000000
    kelon_encoder->kelon_leading_symbol = (rmt_symbol_word_t) { 
        .duration0 = (uint16_t)(kKelonHdrMark * config->resolution / 1000000ULL), // 使用 kKelonHdrMark
        .level0 = 1,
        .duration1 = (uint16_t)(kKelonHdrSpace * config->resolution / 1000000ULL), // 使用 kKelonHdrSpace
        .level1 = 0
    };

    // Kelon 结束符号，通常是一个短高电平后跟一个很长的低电平间隔
    kelon_encoder->kelon_ending_symbol = (rmt_symbol_word_t) { // 改名
        .duration0 = (uint16_t)(kKelonBitMark * config->resolution / 1000000ULL), // 使用 kKelonBitMark
        .level0 = 1,
        .duration1 = (uint16_t)(kKelonGap * config->resolution / 1000000ULL), // 使用 kKelonGap
        // .duration1 = 0x7FFF, // 使用 kKelonGap
        .level1 = 0
    };

    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &kelon_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    *ret_encoder = &kelon_encoder->base;
    return ESP_OK;

err:
    // 错误处理，释放已分配的资源
    if (kelon_encoder) { 
        if (kelon_encoder->bytes_encoder) { 
            rmt_del_encoder(kelon_encoder->bytes_encoder); 
        }
        if (kelon_encoder->copy_encoder) { 
            rmt_del_encoder(kelon_encoder->copy_encoder); 
        }
        free(kelon_encoder); 
    }
    return ret;
}






//发送函数
void bsp_ir_send(uint64_t data)
{
    rmt_transmit(tx_channel, ir_encoder, &data, sizeof(data), &transmit_config);
}
