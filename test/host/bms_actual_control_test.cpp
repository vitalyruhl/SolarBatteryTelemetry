#include "bms/BmsCommandPolicy.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace bms;
// [MOCKED!] Decoded frames; command policy and wire encoder are production code.
static MosRequest request(MosTarget target,bool value){MosRequest r;r.target=target;r.state=value;r.deadline=30000;std::memcpy(r.id,"actual-mos",sizeof("actual-mos"));return r;}
static Frame status(unsigned actual,unsigned alarm=0){Frame f;f.reg=3;f.size=23;f.payload[20]=actual;f.payload[16]=alarm>>8;f.payload[17]=alarm;return f;}
static void ack(BmsCommandPolicy& p,unsigned now){p.markWriteSent();Frame f;f.reg=0xe1;p.response(f,true,{},now);}
int main(){
 struct Case{unsigned actual;MosTarget target;bool value;unsigned disabled;};
 const Case cases[]={
  {0,MosTarget::Charge,false,3},{0,MosTarget::Charge,true,2},{1,MosTarget::Charge,false,3},{1,MosTarget::Charge,true,2},
  {2,MosTarget::Charge,false,1},{2,MosTarget::Charge,true,0},{3,MosTarget::Charge,false,1},{3,MosTarget::Charge,true,0},
  {0,MosTarget::Discharge,false,3},{0,MosTarget::Discharge,true,1},{1,MosTarget::Discharge,false,2},{1,MosTarget::Discharge,true,0},
  {2,MosTarget::Discharge,false,3},{2,MosTarget::Discharge,true,1},{3,MosTarget::Discharge,false,2},{3,MosTarget::Discharge,true,0}};
 for(const auto& c:cases){BmsCommandPolicy p;uint8_t wire[9];size_t size=0;
  assert(!p.command(wire,size));assert(p.submit(request(c.target,c.value),0,true,true));
  assert(p.command(wire,size)&&size==7&&wire[2]==3);p.response(status(c.actual),true,{},1);
  assert(p.command(wire,size)&&size==9&&wire[5]==c.disabled);ack(p,2);
  assert(!p.pending()&&!p.command(wire,size));
 }
 // A previous ACK must not override a later actual pre-read.
 BmsCommandPolicy p;auto r=request(MosTarget::Both,true);r.dischargeState=false;
 assert(p.submit(r,0,true,true));p.response(status(3),true,{},1);ack(p,2);
 r=request(MosTarget::Charge,false);assert(p.submit(r,3,true,true));p.response(status(3),true,{},4);
 uint8_t wire[9];size_t size=0;assert(p.command(wire,size)&&wire[5]==1);
 // A disabled gate blocks admission and cancellation never retries an uncertain write.
 BmsCommandPolicy disabled;assert(!disabled.submit(request(MosTarget::Charge,false),0,false,true));assert(!disabled.command(wire,size));
 p.cancel("disabled");assert(!p.command(wire,size));
 std::puts("Actual MOS startup, all directional pairs, fresh opposite state and gate rejection passed");
}
