#ifdef BOARD_WAVESHARE_P4_KNOB
#include "app.h"
#include "p4_radio.h"
#include "p4_gatt.h"
#include "missions.h"
#include <esp_gattc_api.h>
#include <esp_heap_caps.h>
#include <freertos/event_groups.h>
#include <atomic>

// Bluedroid normally subscribes to Service Changed during discovery. This
// inspector only inventories metadata, including on peripherals with that CCC.
extern "C" uint8_t __wrap_GATTC_Write(uint16_t, uint8_t, void*) {
    return ESP_GATT_WRITE_NOT_PERMIT;
}

namespace {
constexpr uint16_t appId=0xD4;
static_assert(appId<=ESP_APP_ID_MAX,"Bluedroid app ID out of range");
constexpr int noConnection=-1;
enum Events : EventBits_t { Registered=1, RegError=2, Opened=4, OpenError=8,
    Discovered=16, DiscoveryError=32, Closed=64, CancelledOpen=128 };
portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
P4GattView* view=nullptr;
StaticEventGroup_t eventStorage;
EventGroupHandle_t events=nullptr;
std::atomic<bool> active{false},cancelled{false},draining{false},registering{false};
std::atomic<int> client{-1},connection{noConnection};
bool callbackInstalled=false;
std::atomic<const char*> stage{"idle"};
std::atomic<int> apiCode{ESP_OK},registration{-1},openCode{-1},discoveryCode{-1},databaseCode{-1},
                 closeReason{-1},linkSeen{0},rawCount{-1};
std::atomic<uint32_t> began{0},elapsed{0};

bool isTarget(const uint8_t* address){
    taskENTER_CRITICAL(&lock);
    const bool same=view && !memcmp(view->address,address,6);
    taskEXIT_CRITICAL(&lock);
    return same;
}

void callback(esp_gattc_cb_event_t event,esp_gatt_if_t iface,esp_ble_gattc_cb_param_t* p){
    if(event==ESP_GATTC_REG_EVT){
        if(p->reg.app_id!=appId)return;
        registration.store(int(p->reg.status));
        if(p->reg.status==ESP_GATT_OK)client.store(iface);
        registering.store(false);
        xEventGroupSetBits(events,p->reg.status==ESP_GATT_OK?Registered:RegError);
        return;
    }
    if(iface!=client.load() && iface!=ESP_GATT_IF_NONE)return;
    switch(event){
    case ESP_GATTC_CONNECT_EVT:
        if(active.load() && isTarget(p->connect.remote_bda))linkSeen.store(1);
        break;
    case ESP_GATTC_OPEN_EVT:
        if(!isTarget(p->open.remote_bda))break;
        openCode.store(int(p->open.status));
        if(p->open.status==ESP_GATT_OK){
            if(!active.load() && !draining.load()){
                esp_ble_gattc_close(esp_gatt_if_t(client.load()),p->open.conn_id);
                break;
            }
            connection.store(p->open.conn_id);
            xEventGroupSetBits(events,Opened);
            if(draining.load())esp_ble_gattc_close(esp_gatt_if_t(client.load()),p->open.conn_id);
        }else{
            xEventGroupSetBits(events,OpenError);
            draining.store(false);
        }
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if(p->dis_srvc_cmpl.conn_id==connection.load()){
            discoveryCode.store(int(p->dis_srvc_cmpl.status));
            xEventGroupSetBits(events,p->dis_srvc_cmpl.status==ESP_GATT_OK?Discovered:DiscoveryError);
        }
        break;
    case ESP_GATTC_CANCEL_OPEN_EVT:
        if(p->cancel_open.status==ESP_GATT_OK && isTarget(p->cancel_open.remote_bda) && connection.load()==noConnection){
            xEventGroupSetBits(events,CancelledOpen);
            draining.store(false);
        }
        break;
    case ESP_GATTC_CLOSE_EVT:
        if(p->close.status==ESP_GATT_OK && isTarget(p->close.remote_bda) && p->close.conn_id==connection.load()){
            connection.store(noConnection);
            xEventGroupSetBits(events,Closed);
            draining.store(false);
        }
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        if(isTarget(p->disconnect.remote_bda) && p->disconnect.conn_id==connection.load()){
            closeReason.store(int(p->disconnect.reason));
            connection.store(noConnection);
            xEventGroupSetBits(events,Closed);
            draining.store(false);
        }
        break;
    default: break;
    }
}

void progress(const char* message){
    taskENTER_CRITICAL(&lock);
    strlcpy(view->status,message,sizeof(view->status));
    ++view->revision;
    taskEXIT_CRITICAL(&lock);
}

EventBits_t waitFor(EventBits_t mask,uint32_t timeout,bool (*wanted)()){
    const uint32_t start=millis();
    do {
        if(wanted && (cancelled.load() || !wanted()))return 0;
        const auto bits=xEventGroupWaitBits(events,mask,pdFALSE,pdFALSE,pdMS_TO_TICKS(50))&mask;
        if(bits)return bits;
    }while(millis()-start<timeout);
    return 0;
}

bool ensureClient(bool (*wanted)()){
    if(client.load()>=0)return true;
    if(!callbackInstalled){
        stage.store("register_callback");
        apiCode.store(esp_ble_gattc_register_callback(callback));
        if(apiCode.load()!=ESP_OK)return false;
        callbackInstalled=true;
    }
    if(!registering.exchange(true)){
        xEventGroupClearBits(events,Registered|RegError);
        stage.store("app_register");apiCode.store(esp_ble_gattc_app_register(appId));
        if(apiCode.load()!=ESP_OK){registering.store(false);return false;}
    }
    stage.store("registration_event");
    return client.load()>=0 || (waitFor(Registered|RegError,2000,wanted)&Registered);
}

void uuidText(const esp_bt_uuid_t& uuid,char (&out)[37]){
    if(uuid.len==ESP_UUID_LEN_16)snprintf(out,sizeof(out),"%04X",unsigned(uuid.uuid.uuid16));
    else if(uuid.len==ESP_UUID_LEN_32)snprintf(out,sizeof(out),"%08lX",(unsigned long)uuid.uuid.uuid32);
    else if(uuid.len==ESP_UUID_LEN_128){
        char* next=out;
        for(int i=15;i>=0;--i){
            snprintf(next,3,"%02X",unsigned(uuid.uuid.uuid128[i]));next+=2;
            if(i==12 || i==10 || i==8 || i==6)*next++='-';
        }
        *next=0;
    }else strlcpy(out,"Unknown UUID",sizeof(out));
}

bool inventory(esp_gatt_if_t iface,uint16_t link){
    constexpr size_t capacity=sizeof(view->entries)/sizeof(view->entries[0]);
    uint16_t count=capacity+1;
    auto* attributes=static_cast<esp_gattc_db_elem_t*>(heap_caps_calloc(count,sizeof(esp_gattc_db_elem_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!attributes){apiCode.store(ESP_ERR_NO_MEM);return false;}
    const auto result=esp_ble_gattc_get_db(iface,link,1,UINT16_MAX,attributes,&count);
    databaseCode.store(int(result));rawCount.store(result==ESP_GATT_OK?count:0);
    if(result==ESP_GATT_OK){
        uint8_t used=0;
        for(unsigned i=0;i<count && used<capacity;++i){
            const auto& a=attributes[i];
            P4GattEntry e{};
            switch(a.type){
            case ESP_GATT_DB_PRIMARY_SERVICE:case ESP_GATT_DB_SECONDARY_SERVICE:e.kind=P4GattKind::Service;break;
            case ESP_GATT_DB_CHARACTERISTIC:e.kind=P4GattKind::Characteristic;break;
            case ESP_GATT_DB_DESCRIPTOR:e.kind=P4GattKind::Descriptor;break;
            default:continue;
            }
            e.handle=e.kind==P4GattKind::Service?a.start_handle:a.attribute_handle;
            e.properties=e.kind==P4GattKind::Characteristic?a.properties:0;
            uuidText(a.uuid,e.uuid);
            taskENTER_CRITICAL(&lock);
            view->entries[used++]=e;
            taskEXIT_CRITICAL(&lock);
        }
        taskENTER_CRITICAL(&lock);
        view->count=used;view->truncated=count>capacity;
        taskEXIT_CRITICAL(&lock);
    }
    free(attributes);
    return result==ESP_GATT_OK && count!=0;
}

void disconnect(esp_gatt_if_t iface){
    // The GAP worker can resume after a bounded drain. Keep the old peer reserved
    // until Bluedroid reports its cancellation or disconnection, even on timeout.
    draining.store(true);
    const auto terminal=OpenError|Closed|CancelledOpen;
    if(xEventGroupGetBits(events)&terminal){draining.store(false);return;}
    int id=connection.load();
    if(id>=0)esp_ble_gattc_close(iface,uint16_t(id));
    else{
        esp_ble_gattc_cancel_open_params_t request{};
        request.gattc_if=iface;memcpy(request.remote_bda,view->address,6);
        esp_ble_gattc_cancel_open(&request);
    }
    waitFor(terminal,1800,nullptr);
}
}

bool p4GattBusy(){return active.load();}

const char* p4GattUnavailable(const uint8_t address[6],uint8_t type){
    if(active.load())return "Discovery is in progress.";
    if(draining.load())return "The previous link is closing.";
    if(!p4RadarBluetoothAvailable() || p4RadarScanMode()==1)return "Enable Bluetooth in Modes.";
    if(missionActive())return "Stop and save the recon session first.";
    if(radioStandby() || !radioEnabled || p4CoprocBusy())return "The radio is paused or busy.";
    if(!address || type>3)return "Select a Bluetooth contact.";
    dragon::DeviceKey key{};key.radio=1;key.addressType=type;memcpy(key.mac,address,6);
    const int i=findContact(key);
    if(i<0 || millis()-contacts[i].seen>30000)return "Wait for a fresh advertisement.";
    if(!contacts[i].connectable)return "This advertisement is non-connectable.";
    return nullptr;
}

bool p4GattStart(const uint8_t address[6],uint8_t type){
    if(p4GattUnavailable(address,type))return false;
    if(!view){
        auto* memory=static_cast<P4GattView*>(heap_caps_calloc(1,sizeof(P4GattView),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!memory)return false;
        taskENTER_CRITICAL(&lock);view=memory;taskEXIT_CRITICAL(&lock);
    }
    if(!events)events=xEventGroupCreateStatic(&eventStorage);
    if(!events)return false;
    taskENTER_CRITICAL(&lock);
    memcpy(view->address,address,6);view->addressType=type;
    view->count=0;view->complete=false;view->truncated=false;view->busy=true;
    strlcpy(view->status,"Waiting for the radio",sizeof(view->status));++view->revision;
    taskEXIT_CRITICAL(&lock);
    cancelled.store(false);
    stage.store("queued");apiCode.store(ESP_OK);openCode.store(-1);discoveryCode.store(-1);
    databaseCode.store(-1);rawCount.store(-1);closeReason.store(-1);linkSeen.store(0);
    began.store(millis());elapsed.store(0);
    active.store(true);
    p4RadioWakeForGatt();
    return true;
}
void p4GattCancel(){if(active.load())cancelled.store(true);}
void p4GattSnapshot(P4GattView& out){
    taskENTER_CRITICAL(&lock);
    if(view)out=*view;
    else memset(&out,0,sizeof(out));
    taskEXIT_CRITICAL(&lock);
}

void p4GattRun(bool (*wanted)(),bool radioIdle){
    const char* result="Radio unavailable";
    bool complete=false;
    if(!radioIdle){
        stage.store("radio_quiesce");
        result="Could not pause the receiver";
    }else if(wanted() && !cancelled.load() && !ensureClient(wanted)){
        result="Bluetooth client registration failed";
    }else if(wanted() && !cancelled.load() && client.load()>=0){
        const auto iface=esp_gatt_if_t(client.load());
        xEventGroupClearBits(events,Opened|OpenError|Discovered|DiscoveryError|Closed|CancelledOpen);
        connection.store(noConnection);
        progress("Connecting to the selected device");
        stage.store("cache_refresh");
        apiCode.store(esp_ble_gattc_cache_refresh(view->address));
        result="Could not refresh the service cache";
        bool submitted=false;
        if(apiCode.load()==ESP_OK && !cancelled.load() && wanted()){
            stage.store("open_request");
            apiCode.store(esp_ble_gattc_open(iface,view->address,esp_ble_addr_type_t(view->addressType),true));
            result="Bluetooth connection request failed";
            submitted=apiCode.load()==ESP_OK;
        }
        if(submitted){
            stage.store("open_event");
            const auto opened=waitFor(Opened|OpenError|Closed,9500,wanted);
            result="Device did not accept the connection";
            if((opened&Opened) && !(opened&(Closed|OpenError))){
                progress("Discovering services and characteristics");
                stage.store("discovery_event");
                const auto bits=waitFor(Discovered|DiscoveryError|Closed,12000,wanted);
                const int id=connection.load();
                if((bits&Discovered) && !(bits&(Closed|DiscoveryError)) && id>=0){
                    stage.store("inventory");
                    complete=inventory(iface,uint16_t(id));
                }
                result=complete?"Discovery complete":"Service inventory unavailable";
            }
            progress("Closing the inspection link");
            disconnect(iface);
        }
    }
    if(cancelled.load() || !wanted()){complete=false;result="Discovery cancelled";stage.store("cancelled");}
    else if(complete)stage.store("complete");
    elapsed.store(millis()-began.load());
    taskENTER_CRITICAL(&lock);
    if(!complete)view->count=0;
    view->complete=complete;view->busy=false;
    strlcpy(view->status,result,sizeof(view->status));++view->revision;
    taskEXIT_CRITICAL(&lock);
    active.store(false);
}

void p4GattPrintInventory(bool privacy){
    dragon::DeviceKey key{};key.radio=1;
    unsigned count=0;bool complete=false;
    taskENTER_CRITICAL(&lock);
    if(view && view->complete && !view->busy){
        complete=true;count=view->count;
        key.addressType=view->addressType;memcpy(key.mac,view->address,6);
    }
    taskEXIT_CRITICAL(&lock);
    if(!complete){Serial.printf("LAB_ERROR no completed inventory\n");return;}
    const int contact=findContact(key);
    Serial.printf("LAB_INVENTORY contact=%d count=%u\n",contact>=0?contact+1:0,count);
    for(unsigned i=0;i<count;++i){
        P4GattEntry e;
        taskENTER_CRITICAL(&lock);e=view->entries[i];taskEXIT_CRITICAL(&lock);
        const char* kind=e.kind==P4GattKind::Service?"service":
                         e.kind==P4GattKind::Characteristic?"characteristic":"descriptor";
        Serial.printf("LAB_ATTRIBUTE kind=%s handle=%u properties=%02X uuid=%s\n",kind,
                      unsigned(e.handle),unsigned(e.properties),privacy&&strlen(e.uuid)>4?"hidden":e.uuid);
    }
    Serial.printf("LAB_END\n");
}

void p4GattPrintStatus(){
    unsigned services=0,chars=0,descriptors=0;bool complete=false,truncated=false;
    taskENTER_CRITICAL(&lock);
    if(view){
        complete=view->complete;truncated=view->truncated;
        for(unsigned i=0;i<view->count;++i){
            switch(view->entries[i].kind){
            case P4GattKind::Service:++services;break;
            case P4GattKind::Characteristic:++chars;break;
            case P4GattKind::Descriptor:++descriptors;break;
            }
        }
    }
    taskEXIT_CRITICAL(&lock);
    Serial.printf("P4GATT_STATUS busy=%u complete=%u truncated=%u services=%u characteristics=%u descriptors=%u closing=%u "
        "stage=%s api=%d registration=%d interface=%d link_seen=%d open=%d discovery=%d database=%d raw_count=%d "
        "disconnect=%d elapsed_ms=%lu\n",unsigned(active.load()),unsigned(complete),unsigned(truncated),services,chars,
        descriptors,unsigned(draining.load()),stage.load(),apiCode.load(),registration.load(),client.load(),linkSeen.load(),
        openCode.load(),discoveryCode.load(),databaseCode.load(),rawCount.load(),closeReason.load(),
        (unsigned long)(active.load()?millis()-began.load():elapsed.load()));
}
#endif
