// [MOCKED!] Clock, BLE transport and runtime/UI registration only.
#pragma once
#include "cm_platform_adapter.h"
#include <mutex>
#include <cstdint>
inline uint32_t hostNow=0;
inline uint32_t millis(){return hostNow;}
class HostRuntime {public: std::function<void(JsonObject&)> provider;template<class F> void addRuntimeProvider(const char*,F f,int){provider=f;}};
namespace cm {
struct BleAddress {char text[18]{};int type=0;};
enum class BleEventType {Connected,Disconnected,Error,Notification,Advertisement};
struct BleEvent {BleAddress peer;BleEventType type;uint32_t generation=0,queryId=0;int errorCode=0;size_t size=0;uint8_t data[262]{};};
class BluetoothManager {public:
 struct Buffered {uint32_t generation;std::vector<uint8_t> data;};std::vector<Buffered> buffered;bool bufferWrites=false;
 void cancelPendingCommands(){++gen;}
 void drainWrites(){for(const auto& item:buffered)if(item.generation==gen&&item.data.size()==9&&item.data[2]==0xe1)++mosWrites;buffered.clear();}
 unsigned mosWrites=0;bool paused=false,writeAccepted=true,completionAccepted=true;uint32_t gen=1;std::vector<uint8_t> lastWrite;
 bool isPaused()const{return paused;} uint32_t generation()const{return gen;}
 bool configureGattPeer(const BleAddress&,int,int,int){++gen;return true;}
 void clearGattPeer(){++gen;} bool requestConnect(){return true;}
 bool writeCommand(const uint8_t* data,size_t size){lastWrite.assign(data,data+size);if(writeAccepted&&bufferWrites)buffered.push_back({gen,lastWrite});else if(writeAccepted&&size==9&&data[2]==0xe1)++mosWrites;return writeAccepted;}
 bool completeQuery(){return completionAccepted;}
};
}
