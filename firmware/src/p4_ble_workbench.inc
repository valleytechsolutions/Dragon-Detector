#include "p4_advert_fields.h"
#include "p4_gatt.h"

static constexpr int p4ChangesOpen=-22700,p4BaselinePin=-22701,p4BaselineReset=-22702;
static constexpr int p4GattChoose=-22703,p4GattOpen=-22704,p4GattRunAction=-22705;
static lv_obj_t *p4ResearchText=nullptr,*p4ResearchButton=nullptr,*p4BaselineButton=nullptr;
static P4GattView p4GattView{};
static uint32_t p4RenderedRevision=0;
static bool p4RenderedEligible=false;
static struct {
  bool set=false;
  uint8_t mac[6]{},type=0,bytes[dragon::p4::LegacyAdvertBytes]{},length=0;
} p4Baseline;

static const dragon::notebook::Advert* p4LatestAdvert(){
  if(!notebookSelection.used || !notebookAdverts)return nullptr;
  for(unsigned i=0;i<dragon::notebook::Capacity;++i){
    const auto& a=notebookAdverts[i];
    if(a.used && a.type==notebookSelection.type && !memcmp(a.mac,notebookSelection.mac,6))return &a;
  }
  return nullptr;
}
static void p4AppendHex(String& text,const uint8_t* data,unsigned size){
  for(unsigned i=0;i<size;++i){
    char hex[4];snprintf(hex,sizeof(hex),"%02X",unsigned(data[i]));
    if(i)text+=' ';
    text+=hex;
  }
}
static void p4RefreshChanges(bool force){
  const auto* live=p4LatestAdvert();
  const auto& current=live?*live:notebookSelection;
  const bool pinned=p4Baseline.set && p4Baseline.type==current.type && !memcmp(p4Baseline.mac,current.mac,6);
  if(!force && p4RenderedRevision==current.count && p4RenderedEligible==bool(live))return;
  p4RenderedRevision=current.count;p4RenderedEligible=bool(live);
  if(pinned)lv_obj_clear_state(p4BaselineButton,LV_STATE_DISABLED);else lv_obj_add_state(p4BaselineButton,LV_STATE_DISABLED);
  if(live)lv_obj_clear_state(p4ResearchButton,LV_STATE_DISABLED);else lv_obj_add_state(p4ResearchButton,LV_STATE_DISABLED);
  char summary[120];
  snprintf(summary,sizeof(summary),"%lu samples / %lu payload changes\n%s",
      (unsigned long)current.count,(unsigned long)current.changes,
      live?"Last retained advertisement":"Contact expired / last saved view");
  String out=summary;
  dragon::p4::AdvertFields before,after;
  if(!after.parse(current.data,current.boundary)){setText(p4ResearchText,"Complete legacy advertisement unavailable.");return;}
  if(pinned || current.hasPrevious){
    const bool valid=pinned?before.parse(p4Baseline.bytes,p4Baseline.length):
                            before.parse(current.previous,current.previousLength);
    out+=pinned?"\n\nCompared with pinned sample":"\n\nCompared with previous payload";
    if(valid){
      const unsigned changes=dragon::p4::compareFields(before,after,[&](dragon::p4::FieldChange kind,const dragon::p4::AdvertField* oldField,const dragon::p4::AdvertField* newField){
        const auto& field=newField?*newField:*oldField;
        const char* change=kind==dragon::p4::FieldChange::Added?"Added":
                           kind==dragon::p4::FieldChange::Removed?"Removed":"Changed";
        char line[90];snprintf(line,sizeof(line),"\n%s %s (0x%02X, #%u)",change,
            dragon::p4::advertFieldName(field.type),unsigned(field.type),unsigned(field.occurrence));
        out+=line;
        if(!detector.privacy){
          if(oldField){out+="\n  Before: ";p4AppendHex(out,oldField->value,oldField->size);}
          if(newField){out+="\n  After: ";p4AppendHex(out,newField->value,newField->size);}
        }
      });
      if(!changes)out+="\nSame field values.";
    }
  }else out+="\n\nWaiting for a different payload. Pin this sample to compare future broadcasts.";
  out+="\n\nFields in this sample";
  if(!after.count)out+="\nNo fields.";
  for(unsigned i=0;i<after.count;++i){
    const auto& f=after.items[i];
    char line[88];snprintf(line,sizeof(line),"\n0x%02X  %s  / %u B",unsigned(f.type),dragon::p4::advertFieldName(f.type),unsigned(f.size));
    out+=line;
    if(!detector.privacy && f.size){out+='\n';p4AppendHex(out,f.value,f.size);}
  }
  setText(p4ResearchText,out.c_str());
}

static void p4RefreshGatt(bool force){
  p4GattSnapshot(p4GattView);
  const auto& view=p4GattView;
  const bool own=notebookSelection.used && notebookSelection.type==view.addressType && !memcmp(notebookSelection.mac,view.address,6);
  const char* reason=p4GattUnavailable(notebookSelection.mac,notebookSelection.type);
  const bool eligible=!reason;
  const bool cancelling=own && view.busy;
  if(!force && p4RenderedRevision==view.revision && p4RenderedEligible==eligible)return;
  p4RenderedRevision=view.revision;p4RenderedEligible=eligible;
  if(cancelling || eligible)lv_obj_clear_state(p4ResearchButton,LV_STATE_DISABLED);
  else lv_obj_add_state(p4ResearchButton,LV_STATE_DISABLED);
  setText(lv_obj_get_child(p4ResearchButton,0),cancelling?"Cancel":own && view.complete?"Discover again":"Discover services");
  String out;
  if(own && view.status[0])out+=view.status;
  if(!cancelling && reason){if(out.length())out+='\n';out+=reason;}
  if(!out.length())out="Connect once to list this device's services, characteristics and properties.";
  if(own && view.count){
    if(view.truncated)out+="\nInventory limit reached.";
    for(unsigned i=0;i<view.count;++i){
      const auto& attr=view.entries[i];
      const bool service=attr.kind==P4GattKind::Service;
      const char* kind=service?"SERVICE":attr.kind==P4GattKind::Characteristic?"Characteristic":"Descriptor";
      char line[38];snprintf(line,sizeof(line),"%s%s  0x%04X\n",service?"\n\n":"\n",kind,unsigned(attr.handle));
      out+=line;
      if(strlen(attr.uuid)<=4){
        out+=attr.uuid;
        if(const char* name=dragon::lab::uuidName(uint16_t(strtoul(attr.uuid,nullptr,16)))){out+="  ";out+=name;}
      }else out+=detector.privacy?"Custom UUID hidden":attr.uuid;
      if(attr.kind==P4GattKind::Characteristic){
        static const char* bits[]={"Broadcast","Read","Write no response","Write","Notify","Indicate","Signed write","Extended"};
        bool first=true;
        for(unsigned bit=0;bit<8;++bit)if(attr.properties&(1u<<bit)){
          out+=first?"\n":" / ";out+=bits[bit];first=false;
        }
      }
    }
  }
  setText(p4ResearchText,out.c_str());
}

static bool p4BuildResearchPage(){
  if(page!=P4Gatt && page!=P4AdvertChanges)return false;
  char mac[18];displayMac(notebookSelection.mac,mac);
  title(page==P4Gatt?"GATT SERVICES":"ADVERT CHANGES",mac);
  auto* pane=scrollBody(96,156);
  p4ResearchText=label(pane,"",7,0,242,F12);
  if(page==P4Gatt){
    p4ResearchButton=button(root,"Discover services",79,260,202,35,p4GattRunAction);
    p4RefreshGatt(true);
  }else{
    p4ResearchButton=button(root,"Pin sample",65,260,111,35,p4BaselinePin);
    p4BaselineButton=button(root,"Unpin",184,260,111,35,p4BaselineReset);
    p4RefreshChanges(true);
  }
  return true;
}
static bool p4ResearchAction(int id){
  if(page==P4BleLab && id==p4GattChoose){notebookPage=0;go(P4GattPicker);return true;}
  if(page==NotebookDetail && id==p4ChangesOpen){go(P4AdvertChanges);return true;}
  if(page==NotebookDetail && id==p4GattOpen){go(P4Gatt);return true;}
  if(page==P4AdvertChanges && (id==p4BaselinePin || id==p4BaselineReset)){
    if(id==p4BaselineReset)p4Baseline.set=false;
    else if(const auto* a=p4LatestAdvert();a && a->boundary<=sizeof(p4Baseline.bytes)){
      p4Baseline.set=true;p4Baseline.type=a->type;p4Baseline.length=uint8_t(a->boundary);
      memcpy(p4Baseline.mac,a->mac,6);memcpy(p4Baseline.bytes,a->data,a->boundary);
    }
    p4RefreshChanges(true);return true;
  }
  if(page==P4Gatt && id==p4GattRunAction){
    p4GattSnapshot(p4GattView);
    if(p4GattView.busy && p4GattView.addressType==notebookSelection.type && !memcmp(p4GattView.address,notebookSelection.mac,6))p4GattCancel();
    else if(!p4GattStart(notebookSelection.mac,notebookSelection.type)){
      const char* why=p4GattUnavailable(notebookSelection.mac,notebookSelection.type);
      p4Message("GATT SERVICES",why?why:"The receiver is busy. Try again after this window.");
    }
    p4RefreshGatt(true);return true;
  }
  return false;
}
