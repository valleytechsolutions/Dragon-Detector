#if defined(BOARD_WAVESHARE_P4_KNOB)
#include "sdkconfig.h"
#if CONFIG_BT_BLUEDROID_ENABLED && CONFIG_ESP_EXT_CONN_BT_ENABLE
#include "p4_ble_passive.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

enum {
    PARAMS_OK=1u<<0, PARAMS_FAILED=1u<<1, START_OK=1u<<2, START_FAILED=1u<<3,
    COMPLETE=1u<<4, STOP_OK=1u<<5, STOP_FAILED=1u<<6
};
// Only the receive task changes this flag after the one-time host startup.
static bool recovery_needed;
static const char *stage="idle";
const char *p4_ble_window_stage(void) { return stage; }
static esp_ble_scan_params_t scan_params={
    .scan_type=BLE_SCAN_TYPE_PASSIVE, .own_addr_type=BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy=BLE_SCAN_FILTER_ALLOW_ALL, .scan_interval=0x50,
    .scan_window=0x30, .scan_duplicate=BLE_SCAN_DUPLICATE_DISABLE
};
static EventGroupHandle_t events;
static StaticEventGroup_t event_storage;
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static bool accepting;
static uint32_t window_started_ms;
static bool start_confirmed;
static p4_ble_snapshot_t batch;

static void ack(esp_bt_status_t status,EventBits_t ok,EventBits_t failed) {
    xEventGroupSetBits(events,status==ESP_BT_STATUS_SUCCESS?ok:failed);
}

enum { IBEACON=1, EDDYSTONE=2 };
static bool eddystone_frame(const uint8_t* data,size_t n) {
    if (n<2) return false;
    switch (data[0]) {
    case 0x00: return n==18 || (n==20 && !data[18] && !data[19]);
    case 0x10:
        if (n<4 || n>20 || data[2]>3) return false;
        for (size_t i=3;i<n;++i)
            if ((data[i]>0x0d && data[i]<0x21) || data[i]>0x7e) return false;
        return true;
    case 0x20: return (!data[1] && n==14) || (data[1]==1 && n==18);
    case 0x30: return n==10;
    default: return false;
    }
}

static uint8_t read_fields(p4_ble_sighting_t *item,const uint8_t *data,size_t size) {
    uint8_t kinds=0;
    bool completeName=false;
    for (size_t offset=0;offset<size;) {
        size_t len=data[offset++];
        if (!len) break;
        if (len>size-offset) return 0;
        const uint8_t type=data[offset];
        const uint8_t* value=data+offset+1;
        const size_t n=len-1;
        if (!completeName && (type==ESP_BLE_AD_TYPE_NAME_CMPL || type==ESP_BLE_AD_TYPE_NAME_SHORT) && n) {
            const size_t copied=n<sizeof(item->name)?n:sizeof(item->name);
            memcpy(item->name,value,copied);
            item->name_length=(uint8_t)copied;
            completeName=type==ESP_BLE_AD_TYPE_NAME_CMPL;
        } else if (type==0xff && n==25 && value[0]==0x4c && value[1]==0 &&
                   value[2]==2 && value[3]==0x15) {
            kinds|=IBEACON;
        } else if (type==0x16 && n>=3 && value[0]==0xaa && value[1]==0xfe &&
                   eddystone_frame(value+2,n-2)) kinds|=EDDYSTONE;
        offset+=len;
    }
    return kinds;
}

static void gap(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t *p) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: ack(p->scan_param_cmpl.status,PARAMS_OK,PARAMS_FAILED);return;
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            taskENTER_CRITICAL(&guard);
            if (accepting && p->scan_start_cmpl.status==ESP_BT_STATUS_SUCCESS) {
                window_started_ms=(uint32_t)(esp_timer_get_time()/1000);
                start_confirmed=true;
            }
            taskEXIT_CRITICAL(&guard);
            ack(p->scan_start_cmpl.status,START_OK,START_FAILED);return;
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: ack(p->scan_stop_cmpl.status,STOP_OK,STOP_FAILED);return;
        case ESP_GAP_BLE_SCAN_RESULT_EVT: break;
        default: return;
    }
    if (p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        taskENTER_CRITICAL(&guard);
        if (accepting && start_confirmed) {
            batch.captured_ms=(uint32_t)(esp_timer_get_time()/1000);
            uint32_t elapsed=batch.captured_ms-window_started_ms;
            batch.listen_ms=elapsed<2000?elapsed:2000;
        }
        taskEXIT_CRITICAL(&guard);
        xEventGroupSetBits(events,COMPLETE);
        return;
    }
    if (p->scan_rst.search_evt!=ESP_GAP_SEARCH_INQ_RES_EVT || p->scan_rst.rssi>=0 ||
        p->scan_rst.rssi < -127 || p->scan_rst.adv_data_len>31 ||
        p->scan_rst.ble_evt_type==ESP_BLE_EVT_SCAN_RSP) return;
    p4_ble_sighting_t observed={0};
    observed.seen_ms=(uint32_t)(esp_timer_get_time()/1000);
    observed.reports=1;
    memcpy(observed.address,p->scan_rst.bda,sizeof(observed.address));
    observed.address_type=p->scan_rst.ble_addr_type;
    observed.connectable=p->scan_rst.ble_evt_type==ESP_BLE_EVT_CONN_ADV ||
                         p->scan_rst.ble_evt_type==ESP_BLE_EVT_CONN_DIR_ADV;
    observed.rssi=(int8_t)p->scan_rst.rssi;
    observed.payload_length=(uint8_t)p->scan_rst.adv_data_len;
    memcpy(observed.payload,p->scan_rst.ble_adv,observed.payload_length);
    const uint8_t kinds=read_fields(&observed,p->scan_rst.ble_adv,p->scan_rst.adv_data_len);
    taskENTER_CRITICAL(&guard);
    if (accepting) {
        ++batch.reports;
        if (kinds&IBEACON) ++batch.ibeacon_reports;
        if (kinds&EDDYSTONE) ++batch.eddystone_reports;
        uint8_t slot=0;
        for (;slot<batch.count;++slot) {
            if (batch.sightings[slot].address_type==observed.address_type &&
                !memcmp(batch.sightings[slot].address,observed.address,sizeof(observed.address))) break;
        }
        if (slot<P4_BLE_MAX_SIGHTINGS) {
            if (slot==batch.count) ++batch.count;
            else {
                observed.reports+=batch.sightings[slot].reports;
                if (!observed.name_length && batch.sightings[slot].name_length) {
                    observed.name_length=batch.sightings[slot].name_length;
                    memcpy(observed.name,batch.sightings[slot].name,observed.name_length);
                }
            }
            batch.sightings[slot]=observed;
        }
    }
    taskEXIT_CRITICAL(&guard);
}

static esp_err_t wait_ack(EventBits_t ok,EventBits_t failed,uint32_t ms) {
    EventBits_t bits=xEventGroupWaitBits(events,ok|failed,pdFALSE,pdFALSE,pdMS_TO_TICKS(ms));
    if (bits&failed) return ESP_FAIL;
    return bits&ok ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t set_passive_params(void) {
    xEventGroupClearBits(events,PARAMS_OK|PARAMS_FAILED);
    esp_err_t err=esp_ble_gap_set_scan_params(&scan_params);
    return err==ESP_OK?wait_ack(PARAMS_OK,PARAMS_FAILED,1000):err;
}

esp_err_t p4_ble_listen_init(void) {
    events=xEventGroupCreateStatic(&event_storage);
    if (!events) return ESP_ERR_NO_MEM;
    esp_err_t err=esp_bluedroid_init();
    if (err==ESP_OK) err=esp_bluedroid_enable();
    if (err==ESP_OK) err=esp_ble_gap_register_callback(gap);
    return err==ESP_OK?set_passive_params():err;
}

esp_err_t p4_ble_listen_quiesce(void) {
    if (!recovery_needed) return ESP_OK;
    // Inquiry completion and the explicit STOP response are different GAP
    // events. Wait for the STOP we issued, then establish a fresh passive
    // parameter acknowledgement before opening another result batch.
    xEventGroupClearBits(events,STOP_OK|STOP_FAILED);
    stage="recover_stop";
    esp_err_t err=esp_ble_gap_stop_scanning();
    if (err!=ESP_OK) return err;
    err=wait_ack(STOP_OK,STOP_FAILED,850);
    if (err==ESP_ERR_TIMEOUT) return err;
    // A completed two-second scan may already be idle and reject STOP.
    // In both cases require a successful new parameter transaction.
    stage="recover_parameters";
    err=set_passive_params();
    if (err==ESP_OK) recovery_needed=false;
    return err;
}

esp_err_t p4_ble_listen_window(p4_ble_snapshot_t *out,bool (*wanted)(void)) {
    if (!out || !wanted || !events) return ESP_ERR_INVALID_ARG;
    out->count=0;out->reports=0;out->ibeacon_reports=0;out->eddystone_reports=0;
    out->listen_ms=0;out->captured_ms=0;
    if (!wanted()) return ESP_ERR_INVALID_STATE;
    esp_err_t err=p4_ble_listen_quiesce();
    if (err!=ESP_OK || !wanted()) return err!=ESP_OK?err:ESP_ERR_INVALID_STATE;

    xEventGroupClearBits(events,START_OK|START_FAILED|COMPLETE);
    taskENTER_CRITICAL(&guard);
    batch.count=0;batch.reports=0;batch.ibeacon_reports=0;batch.eddystone_reports=0;accepting=true;
    batch.listen_ms=0;batch.captured_ms=0;start_confirmed=false;
    taskEXIT_CRITICAL(&guard);
    bool cancelled=false;
    stage="start_submit";
    err=esp_ble_gap_start_scanning(2);
    if (err==ESP_OK) {
        recovery_needed=true;
        stage="start_ack";
        err=wait_ack(START_OK,START_FAILED,1100);
        if (err==ESP_OK) {
            stage="window_end";
            const int64_t until=esp_timer_get_time()+4200000;
            do {
                if (!cancelled && !wanted()) {
                    // Do not race the natural end of a finite scan with STOP.
                    // Stop ingesting data immediately, then drain its end event.
                    cancelled=true;
                    taskENTER_CRITICAL(&guard);
                    accepting=false;
                    taskEXIT_CRITICAL(&guard);
                }
                err=wait_ack(COMPLETE,0,50);
            } while (err==ESP_ERR_TIMEOUT && esp_timer_get_time()<until);
            if (err==ESP_OK) recovery_needed=false;
        }
    }
    const bool deliver=err==ESP_OK && !cancelled && wanted();
    taskENTER_CRITICAL(&guard);
    accepting=false;
    if (deliver) {
        *out=batch;
    } else if (err==ESP_OK) err=ESP_ERR_INVALID_STATE;
    taskEXIT_CRITICAL(&guard);
    return err;
}
#endif
#endif
