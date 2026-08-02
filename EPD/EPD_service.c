/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "EPD_service.h"

#include <string.h>

#include "app_scheduler.h"
#include "ble_srv_common.h"
#include "main.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_pwr_mgmt.h"
#include "sdk_macros.h"

#if defined(S112)
#define EPD_CFG_52811 {0x14, 0x13, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02, 0xFF, 0x12, 0x07}
#define EPD_CFG_52810 {0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x02, 0xFF, 0x0D, 0x02}
#else
#define EPD_CFG_DEFAULT {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x03, 0x09, 0x03}
// #define EPD_CFG_DEFAULT {0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x01, 0x07}
#endif

static void epd_gui_update(void* p_event_data, uint16_t event_size) {
    epd_gui_update_event_t* event = (epd_gui_update_event_t*)p_event_data;
    ble_epd_t* p_epd = event->p_epd;

    EPD_GPIO_Init();
    epd_model_t* epd = epd_init((epd_model_id_t)p_epd->config.model_id);
    gui_data_t data = {
        .mode = (display_mode_t)p_epd->config.display_mode,
        .color = epd->color,
        .width = epd->width,
        .height = epd->height,
        .timestamp = event->timestamp,
        .voltage = EPD_ReadVoltage(),
        .dashboard = p_epd->dashboard,
    };

    DrawGUI(&data, (buffer_callback)epd->drv->write_image, epd);
    epd->drv->refresh(epd);
    EPD_GPIO_Uninit();

    app_feed_wdt();
}

static uint16_t read_u16_le(const uint8_t* bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void dashboard_defaults(codex_dashboard_t* dashboard) {
    static const uint16_t history[7] = {76, 93, 58, 52, 68, 61, 88};

    memset(dashboard, 0, sizeof(*dashboard));
    dashboard->today_tokens_tenth_m = 1339;
    dashboard->month_tokens_tenth_m = 4604;
    dashboard->current_streak_days = 8;
    dashboard->longest_streak_days = 14;
    dashboard->primary_used_hundredths_percent = 2500;
    dashboard->secondary_used_hundredths_percent = 4000;
    dashboard->lifetime_tokens_tenth_m = 10234;
    dashboard->peak_daily_tokens_tenth_m = 456;
    memcpy(dashboard->history_tenth_m, history, sizeof(history));
}

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    p_epd->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    EPD_GPIO_Init();
}

/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    UNUSED_PARAMETER(p_ble_evt);
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    if (p_epd->epd != NULL && p_epd->epd->drv != NULL) {
        p_epd->epd->drv->sleep(p_epd->epd);
        nrf_delay_ms(200);
    }
    EPD_GPIO_Uninit();
}

static void epd_update_display_mode(ble_epd_t* p_epd, display_mode_t mode) {
    if (p_epd->config.display_mode != mode) {
        p_epd->config.display_mode = mode;
        epd_config_write(&p_epd->config);
    }
}

static void epd_service_on_write(ble_epd_t* p_epd, uint8_t* p_data, uint16_t length) {
    NRF_LOG_DEBUG("[EPD]: on_write LEN=%d\n", length);
    NRF_LOG_HEXDUMP_DEBUG(p_data, length);
    if (p_data == NULL || length <= 0) return;

    switch (p_data[0]) {
        case EPD_CMD_SET_PINS:
            if (length < 8) return;

            p_epd->config.mosi_pin = p_data[1];
            p_epd->config.sclk_pin = p_data[2];
            p_epd->config.cs_pin = p_data[3];
            p_epd->config.dc_pin = p_data[4];
            p_epd->config.rst_pin = p_data[5];
            p_epd->config.busy_pin = p_data[6];
            p_epd->config.bs_pin = p_data[7];
            if (length > 8) p_epd->config.en_pin = p_data[8];
            epd_config_write(&p_epd->config);

            EPD_GPIO_Uninit();
            EPD_GPIO_Load(&p_epd->config);
            EPD_GPIO_Init();
            break;

        case EPD_CMD_INIT:
            p_epd->epd = epd_init((epd_model_id_t)(length > 1 ? p_data[1] : p_epd->config.model_id));
            if (p_epd->epd->id != p_epd->config.model_id) {
                p_epd->config.model_id = p_epd->epd->id;
                epd_config_write(&p_epd->config);
            }
            break;

        case EPD_CMD_SET_TIME: {
            if (length < 5) return;

            NRF_LOG_DEBUG("time: %02x %02x %02x %02x\n", p_data[1], p_data[2], p_data[3], p_data[4]);
            if (length > 5) NRF_LOG_DEBUG("timezone: %d\n", (int8_t)p_data[5]);

            uint32_t timestamp = (p_data[1] << 24) | (p_data[2] << 16) | (p_data[3] << 8) | p_data[4];
            timestamp += (length > 5 ? (int8_t)p_data[5] : 8) * 60 * 60;  // timezone
            set_timestamp(timestamp);
            epd_update_display_mode(p_epd, MODE_CODEX_DASHBOARD);
            ble_epd_on_timer(p_epd, timestamp, true);
        } break;

        #if 0
        case EPD_CMD_SET_WEEK_START:
            if (length < 2) return;
            if (p_data[1] < 7) {
                p_epd->config.week_start = p_data[1];
                epd_config_write(&p_epd->config);
                // 如果当前是日历模式，立即刷新显示以应用新的星期第一天设置
                if (p_epd->config.display_mode == MODE_CALENDAR) {
                    extern uint32_t timestamp(void);
                    ble_epd_on_timer(p_epd, timestamp(), true);
                }
            }
            break;

        #endif

        case EPD_CMD_SET_DASHBOARD:
            /*
             * BLE payload is split into two packets to support the 20-byte
             * ATT payload of the older nRF51 firmware:
             *   [0x22, 0x00, todayM, monthM, currentStreak, longestStreak, fiveHourUsed,
             *    weekUsed, lifetimeM, peakDayM] (all fields are uint16 little-endian)
             *   [0x22, 0x01, day0M ... day6M] (seven uint16 little-endian values)
             */
            if (length < 2) return;
            if (p_data[1] == 0 && length == 18) {
                p_epd->dashboard.today_tokens_tenth_m = read_u16_le(&p_data[2]);
                p_epd->dashboard.month_tokens_tenth_m = read_u16_le(&p_data[4]);
                p_epd->dashboard.current_streak_days = read_u16_le(&p_data[6]);
                p_epd->dashboard.longest_streak_days = read_u16_le(&p_data[8]);
                p_epd->dashboard.primary_used_hundredths_percent = read_u16_le(&p_data[10]);
                p_epd->dashboard.secondary_used_hundredths_percent = read_u16_le(&p_data[12]);
                p_epd->dashboard.lifetime_tokens_tenth_m = read_u16_le(&p_data[14]);
                p_epd->dashboard.peak_daily_tokens_tenth_m = read_u16_le(&p_data[16]);
            } else if (p_data[1] == 1 && length == 16) {
                for (uint8_t i = 0; i < 7; i++) {
                    p_epd->dashboard.history_tenth_m[i] = read_u16_le(&p_data[2 + i * 2]);
                }
                epd_update_display_mode(p_epd, MODE_CODEX_DASHBOARD);
                ble_epd_on_timer(p_epd, timestamp(), true);
            }
            break;

        #if 0
        case EPD_CMD_WRITE_IMAGE:  // Legacy image-transfer mode removed from the dashboard build.
            if (length < 3) return;
            p_epd->epd->drv->write_ram(p_epd->epd, p_data[1], &p_data[2], length - 2);
            break;

        case EPD_CMD_WRITE_BLOCK: {
            // Data format: [cmd(1)][block_id(2)][total(2)][cfg(1)][payload(N)][crc16(2)]
            if (length < 8) return;  // Minimum length check

            // Parse block_id first (needed for NACK response)
            uint16_t block_id = p_data[1] | (p_data[2] << 8);

            // Validate EPD is initialized
            if (p_epd->epd == NULL || p_epd->epd->drv == NULL) {
                send_block_response(p_epd, block_id, 0x03);  // NACK - EPD not initialized
                break;
            }

            uint16_t total = p_data[3] | (p_data[4] << 8);
            uint8_t cfg = p_data[5];  // Layer + first block flag
            uint16_t payload_len = length - 8;
            uint8_t* payload = &p_data[6];
            uint16_t recv_crc = p_data[length - 2] | (p_data[length - 1] << 8);

            // Validate block_id and total are within limits
            if (total == 0 || total > EPD_MAX_BLOCKS ||
                block_id >= EPD_MAX_BLOCKS || block_id >= total) {
                send_block_response(p_epd, block_id, 0x02);  // NACK - invalid params
                break;
            }

            // Calculate CRC (only verify payload)
            uint16_t calc_crc = crc16_compute(payload, payload_len);

            if (calc_crc == recv_crc) {
                // Initialize transfer context if not active or total_blocks changed
                if (!p_epd->transfer_ctx.transfer_active ||
                    (p_epd->transfer_ctx.total_blocks != total && p_epd->transfer_ctx.total_blocks != 0)) {
                    // Auto switch from clock mode to picture mode
                    epd_update_display_mode(p_epd, MODE_PICTURE);
                    p_epd->transfer_ctx.total_blocks = total;
                    p_epd->transfer_ctx.received_blocks = 0;
                    memset(p_epd->transfer_ctx.block_bitmap, 0, EPD_BLOCK_BITMAP_SIZE);
                    p_epd->transfer_ctx.transfer_active = true;
                }

                // Check if block already received (avoid duplicate)
                uint16_t byte_idx = block_id / 8;
                uint8_t bit_idx = block_id % 8;
                if (byte_idx < EPD_BLOCK_BITMAP_SIZE &&
                    !(p_epd->transfer_ctx.block_bitmap[byte_idx] & (1 << bit_idx))) {
                    // New block: write to EPD RAM using cfg from APP
                    p_epd->epd->drv->write_ram(p_epd->epd, cfg, payload, payload_len);

                    // Mark block as received
                    p_epd->transfer_ctx.block_bitmap[byte_idx] |= (1 << bit_idx);
                    p_epd->transfer_ctx.received_blocks++;
                }

                send_block_response(p_epd, block_id, 0x00);  // ACK
            } else {
                send_block_response(p_epd, block_id, 0x01);  // NACK - CRC error
            }
            app_feed_wdt();
            break;
        }

        case EPD_CMD_QUERY_STATUS:
            send_status_response(p_epd);
            break;

        case EPD_CMD_RESET_TRANSFER:
            if (length >= 2) {
                p_epd->transfer_ctx.session_id = p_data[1];
            }
            p_epd->transfer_ctx.total_blocks = 0;
            p_epd->transfer_ctx.received_blocks = 0;
            memset(p_epd->transfer_ctx.block_bitmap, 0, EPD_BLOCK_BITMAP_SIZE);
            p_epd->transfer_ctx.transfer_active = false;
            break;

        case EPD_CMD_SET_CONFIG:
            if (length < 2) return;
            memcpy(&p_epd->config, &p_data[1], (length - 1 > EPD_CONFIG_SIZE) ? EPD_CONFIG_SIZE : length - 1);
            epd_config_write(&p_epd->config);
            break;

        case EPD_CMD_SYS_SLEEP:
            sleep_mode_enter();
            break;

        case EPD_CMD_SYS_RESET:
#if defined(S112)
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
#else
            NVIC_SystemReset();
#endif
            break;

        case EPD_CMD_CFG_ERASE:
            epd_config_clear(&p_epd->config);
            nrf_delay_ms(100);  // required
            NVIC_SystemReset();
            break;

        #endif

        default:
            break;
    }
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    ble_gatts_evt_write_t* p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if ((p_evt_write->handle == p_epd->char_handles.cccd_handle) && (p_evt_write->len == 2)) {
        if (ble_srv_is_notification_enabled(p_evt_write->data)) {
            NRF_LOG_DEBUG("notification enabled\n");
            p_epd->is_notification_enabled = true;
            static uint16_t length = sizeof(epd_config_t);
            NRF_LOG_DEBUG("send epd config\n");
            uint32_t err_code = ble_epd_string_send(p_epd, (uint8_t*)&p_epd->config, length);
            if (err_code != NRF_ERROR_INVALID_STATE) APP_ERROR_CHECK(err_code);
        } else {
            p_epd->is_notification_enabled = false;
        }
    } else if (p_evt_write->handle == p_epd->char_handles.value_handle) {
        epd_service_on_write(p_epd, p_evt_write->data, p_evt_write->len);
    } else {
        // Do Nothing. This event is not relevant for this service.
    }
}

#if defined(S112)
void ble_epd_evt_handler(ble_evt_t const* p_ble_evt, void* p_context) {
    if (p_context == NULL || p_ble_evt == NULL) return;

    ble_epd_t* p_epd = (ble_epd_t*)p_context;
    ble_epd_on_ble_evt(p_epd, (ble_evt_t*)p_ble_evt);
}
#endif

void ble_epd_on_ble_evt(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    if ((p_epd == NULL) || (p_ble_evt == NULL)) {
        return;
    }

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_epd, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_epd, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_epd, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}

static uint32_t epd_service_init(ble_epd_t* p_epd) {
    ble_uuid_t ble_uuid = {0};
    ble_uuid128_t base_uuid = BLE_UUID_EPD_SVC_BASE;
    ble_add_char_params_t add_char_params;
    uint8_t app_version = APP_VERSION;

    VERIFY_SUCCESS(sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type));

    ble_uuid.type = ble_uuid.type;
    ble_uuid.uuid = BLE_UUID_EPD_SVC;
    VERIFY_SUCCESS(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_epd->service_handle));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid = BLE_UUID_EPD_CHAR;
    add_char_params.uuid_type = ble_uuid.type;
    add_char_params.max_len = BLE_EPD_MAX_DATA_LEN;
    add_char_params.init_len = sizeof(uint8_t);
    add_char_params.is_var_len = true;
    add_char_params.char_props.notify = 1;
    add_char_params.char_props.write = 1;
    add_char_params.char_props.write_wo_resp = 1;
    add_char_params.read_access = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    VERIFY_SUCCESS(characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->char_handles));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid = BLE_UUID_APP_VER;
    add_char_params.uuid_type = ble_uuid.type;
    add_char_params.max_len = sizeof(uint8_t);
    add_char_params.init_len = sizeof(uint8_t);
    add_char_params.p_init_value = &app_version;
    add_char_params.char_props.read = 1;
    add_char_params.read_access = SEC_OPEN;

    return characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->app_ver_handles);
}

void ble_epd_sleep_prepare(ble_epd_t* p_epd) {
    // Turn off led
    EPD_LED_OFF();
    // Prepare wakeup pin
    if (p_epd->config.wakeup_pin != 0xFF) {
        nrf_gpio_cfg_sense_input(p_epd->config.wakeup_pin, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
    }
}

uint32_t ble_epd_init(ble_epd_t* p_epd) {
    if (p_epd == NULL) return NRF_ERROR_NULL;

    // Initialize the service structure.
    p_epd->max_data_len = BLE_EPD_MAX_DATA_LEN;
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_epd->is_notification_enabled = false;

    dashboard_defaults(&p_epd->dashboard);

    epd_config_init(&p_epd->config);
    epd_config_read(&p_epd->config);

    // write default config
    if (epd_config_empty(&p_epd->config)) {
#if defined(S112)
        if (NRF_FICR->INFO.PART == 0x52810) {
            uint8_t cfg[] = EPD_CFG_52810;
            memcpy(&p_epd->config, cfg, sizeof(cfg));
        } else {
            uint8_t cfg[] = EPD_CFG_52811;
            memcpy(&p_epd->config, cfg, sizeof(cfg));
        }
#else
        uint8_t cfg[] = EPD_CFG_DEFAULT;
        memcpy(&p_epd->config, cfg, sizeof(cfg));
#endif
        p_epd->config.display_mode = MODE_CODEX_DASHBOARD;
        epd_config_write(&p_epd->config);
    }

    if (p_epd->config.display_mode != MODE_CODEX_DASHBOARD) {
        p_epd->config.display_mode = MODE_CODEX_DASHBOARD;
        epd_config_write(&p_epd->config);
    }

    // load config
    EPD_GPIO_Load(&p_epd->config);

    // blink LED on start
    EPD_LED_BLINK();

    // Add the service.
    return epd_service_init(p_epd);
}

uint32_t ble_epd_string_send(ble_epd_t* p_epd, uint8_t* p_string, uint16_t length) {
    if ((p_epd->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epd->is_notification_enabled))
        return NRF_ERROR_INVALID_STATE;
    if (length > p_epd->max_data_len) return NRF_ERROR_INVALID_PARAM;

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_epd->char_handles.value_handle;
    hvx_params.p_data = p_string;
    hvx_params.p_len = &length;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;

    return sd_ble_gatts_hvx(p_epd->conn_handle, &hvx_params);
}

void ble_epd_on_timer(ble_epd_t* p_epd, uint32_t timestamp, bool force_update) {
    if (force_update) {
        epd_gui_update_event_t event = {p_epd, timestamp};
        app_sched_event_put(&event, sizeof(epd_gui_update_event_t), epd_gui_update);
    }
}
