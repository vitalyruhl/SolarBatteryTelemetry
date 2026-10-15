#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
namespace telemetry {
inline constexpr std::size_t rootLimit=63,topicLimit=128,payloadLimit=1024;
enum class Source { Bms, Victron };
enum class FieldType { Boolean, Real, Unsigned, State };
enum class Support { PendingAdapter, Unsupported, DerivedLater, InstantReadout };
struct Channel { Source source; FieldType type; const char* suffix; const char* label; const char* unit; Support support=Support::PendingAdapter; };
inline constexpr Channel channels[]={
 {Source::Bms,FieldType::Boolean,"bms/charging","Charging",""},
 {Source::Bms,FieldType::Boolean,"bms/discharging","Discharging",""},
 {Source::Bms,FieldType::Real,"bms/state_of_charge_pct","State of charge","%"},
 {Source::Bms,FieldType::Real,"bms/voltage_v","Battery voltage","V"},
 {Source::Bms,FieldType::Real,"bms/current_a","Battery current","A"},
 {Source::Bms,FieldType::Real,"bms/power_w","Battery power","W"},
 {Source::Bms,FieldType::Real,"bms/remaining_capacity_ah","Remaining capacity","Ah"},
 {Source::Bms,FieldType::Real,"bms/full_capacity_ah","Full capacity","Ah"},
 {Source::Bms,FieldType::Boolean,"bms/charge_enabled","Charge MOS enabled",""},
 {Source::Bms,FieldType::Boolean,"bms/discharge_enabled","Discharge MOS enabled",""},
 {Source::Bms,FieldType::Real,"bms/temperature_max_c","Maximum temperature","C"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_min_v","Minimum cell voltage","V"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_max_v","Maximum cell voltage","V"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_delta_v","Cell voltage spread","V"},
 {Source::Bms,FieldType::Unsigned,"bms/alarm_bits","Alarm bits",""},
 {Source::Victron,FieldType::Real,"victron/solar_power_w","Solar power","W",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/pv_voltage_v","PV voltage (unsupported)","V",Support::Unsupported},
 {Source::Victron,FieldType::Real,"victron/pv_current_a","PV current (unsupported)","A",Support::Unsupported},
 {Source::Victron,FieldType::Real,"victron/battery_voltage_v","Charger battery voltage","V",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/charge_current_a","Charger output current","A",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/battery_power_w","Charger battery power (derived later)","W",Support::DerivedLater},
 {Source::Victron,FieldType::Real,"victron/load_current_a","Load current","A",Support::InstantReadout},
 {Source::Victron,FieldType::State,"victron/charge_state","Charge state","",Support::InstantReadout},
 {Source::Victron,FieldType::Unsigned,"victron/error_code","Error code","",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/yield_today_wh","Yield today","Wh",Support::InstantReadout}
};
inline constexpr std::size_t channelCount=sizeof(channels)/sizeof(channels[0]);
inline constexpr const char* chargeStates[]={"off","fault","bulk","absorption","float","storage","equalization","other","unavailable"};
inline constexpr const char* commandSuffixes[]={"bms/charge_enabled/set","bms/discharge_enabled/set"};
inline const char* sourceName(Source s){return s==Source::Bms?"bms":"victron";}
inline const char* supportName(Support s){return s==Support::InstantReadout?"instant_readout":s==Support::Unsupported?"unsupported":s==Support::DerivedLater?"derived_later":"adapter_missing";}
inline const char* typeName(FieldType t){switch(t){case FieldType::Boolean:return "bool";case FieldType::Real:return "real";case FieldType::Unsigned:return "unsigned";case FieldType::State:return "enum";}return "unknown";}
inline bool validRoot(const char* root,std::size_t n) {
 if(!root||!n||n>rootLimit||root[0]=='/'||root[n-1]=='/')return false;
 for(std::size_t i=0;i<n;++i) {
  const unsigned char c=static_cast<unsigned char>(root[i]);
  // A conservative ASCII prefix also keeps capabilities JSON unambiguous.
  if(c<=32||c>=127||c=='+'||c=='#'||c=='"'||c=='\\'||(c=='/'&&i&&root[i-1]=='/'))return false;
 }
 return true;
}
inline bool buildTopic(const char* root,const char* suffix,char* out,std::size_t capacity) {
 if(!out||!capacity||!root||!suffix)return false;
 out[0]='\0';const auto n=std::strlen(root),s=std::strlen(suffix);
 if(!validRoot(root,n)||!s||n+1+s>topicLimit||n+1+s>=capacity)return false;
 std::memcpy(out,root,n);out[n]='/';std::memcpy(out+n+1,suffix,s+1);return true;
}
struct Value { bool valid=false; double number=0; };
inline bool formatValue(const Channel& c,Value v,char* out,std::size_t capacity) {
 if(!out||!capacity)return false;
 int n=0;
 if(!v.valid||!std::isfinite(v.number)||(c.support==Support::Unsupported||c.support==Support::DerivedLater))n=std::snprintf(out,capacity,"null");
 else switch(c.type) {
  case FieldType::Boolean:n=std::snprintf(out,capacity,v.number==0?"false":v.number==1?"true":"null");break;
  case FieldType::Real:n=std::snprintf(out,capacity,"%.7g",v.number);break;
  case FieldType::Unsigned:
   if(v.number<0||v.number>4294967295.0||std::floor(v.number)!=v.number)n=std::snprintf(out,capacity,"null");
   else n=std::snprintf(out,capacity,"%lu",static_cast<unsigned long>(v.number));break;
  case FieldType::State:
   if(v.number<0||v.number>=9||std::floor(v.number)!=v.number)n=std::snprintf(out,capacity,"null");
   else n=std::snprintf(out,capacity,"\"%s\"",chargeStates[static_cast<unsigned>(v.number)]);break;
 }
 return n>=0&&static_cast<std::size_t>(n)<capacity&&static_cast<std::size_t>(n)<=payloadLimit;
}
inline bool capabilities(const char* root,char* out,std::size_t capacity,bool victronAvailable=false) {
 char a[topicLimit+1],b[topicLimit+1];
 if(!buildTopic(root,commandSuffixes[0],a,sizeof(a))||!buildTopic(root,commandSuffixes[1],b,sizeof(b)))return false;
 const int n=std::snprintf(out,capacity,
  "{\"schema_version\":1,\"read_only\":true,\"commands_enabled\":false,\"reserved_commands\":[\"%s\",\"%s\"],\"source_state\":\"%s\",\"reason\":\"%s\",\"adapters\":{\"bms\":\"missing\",\"victron\":\"instant_readout\"},\"unsupported\":[\"victron/pv_voltage_v\",\"victron/pv_current_a\"],\"derived_later\":[\"victron/battery_power_w\"],\"charge_states\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
  a,b,victronAvailable?"partial":"unavailable",victronAvailable?"bms_adapter_missing":"no_fresh_data",chargeStates[0],chargeStates[1],chargeStates[2],chargeStates[3],chargeStates[4],chargeStates[5],chargeStates[6],chargeStates[7],chargeStates[8]);
 return n>=0&&static_cast<std::size_t>(n)<capacity&&static_cast<std::size_t>(n)<=payloadLimit;
}
}
