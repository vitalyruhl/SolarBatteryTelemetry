#include "bms/BmsCommandPolicy.h"
#include <cassert>
#include <cstdio>
using namespace bms;
int main(){
 // [MOCKED!] Decoded frames at the portable transaction boundary, no radio mock.
 const auto request=[](MosTarget target,bool state){MosRequest r;r.target=target;r.state=state;std::memcpy(r.id,"test-1",7);r.deadline=30000;return r;};
 MosMailbox mailbox;auto input=request(MosTarget::Charge,false);MosRequest dequeued;
 assert(!std::strcmp(mailbox.offer(input,0,false,true,false),"rejected_disabled"));
 assert(!mailbox.take(dequeued));
 assert(!std::strcmp(mailbox.offer(input,0,true,false,false),"rejected_unavailable"));
 assert(!mailbox.take(dequeued));
 assert(!std::strcmp(mailbox.offer(input,0,true,true,true),"rejected_cancelled"));
 assert(!std::strcmp(mailbox.offer(input,30000,true,true,false),"rejected_expired"));
 assert(!std::strcmp(mailbox.offer(input,0,true,true,false),"accepted"));
 assert(mailbox.take(dequeued)); // Reservation persists while the loop starts the transaction.
 assert(!std::strcmp(mailbox.offer(input,1,true,true,false),"rejected_busy"));
 mailbox.settle(true);assert(mailbox.pending());mailbox.settle(false);assert(!mailbox.pending());
 assert(!std::strcmp(mailbox.offer(input,2,true,true,false),"accepted"));
 mailbox.cancel();assert(!mailbox.take(dequeued)&&!mailbox.pending());
 for(auto target:{MosTarget::Charge,MosTarget::Discharge})for(bool state:{false,true})for(uint8_t flags=0;flags<4;++flags){
  BmsCommandPolicy p;assert(!p.submit(request(target,state),0,false,true));assert(!p.pending());
  assert(p.submit(request(target,state),0,true,true));uint8_t command[9];size_t size=0;
  assert(p.command(command,size)&&size==7&&command[2]==3);
  Frame f;f.reg=3;f.size=23;f.payload[20]=flags;p.response(f,true,{},100);
  assert(p.command(command,size)&&size==9&&command[0]==0xdd&&command[1]==0x5a&&command[2]==0xe1&&command[3]==2&&command[4]==0);
  const uint8_t mask=target==MosTarget::Charge?1:2;
  assert((command[5]&mask)==(state?0:mask));assert((command[5]&(3^mask))==((flags^3)&(3^mask)));
  assert(command[6]==0xff&&command[7]==uint8_t(0-(0xe3+command[5]))&&command[8]==0x77);
  Frame ack;ack.reg=0xe1;p.response(ack,true,{},200);assert(p.pending()&&p.command(command,size)&&command[2]==3);
  f.payload[20]=static_cast<uint8_t>((flags&~mask)|(state?mask:0));p.response(f,true,{},210);
  assert(p.command(command,size)&&command[2]==4);
  Snapshot sample;sample.available=true;sample.receivedAt=220;sample.values[8]={true,(f.payload[20]&1)?1.:0.};sample.values[9]={true,(f.payload[20]&2)?1.:0.};
  Frame cells;cells.reg=4;p.response(cells,true,sample,220);assert(!p.pending()&&!std::strcmp(p.result().phase,"verified"));
 }
 BmsCommandPolicy p;auto r=request(MosTarget::Charge,false);assert(!p.submit(r,0,true,false));assert(p.submit(r,0,true,true));
 assert(!p.submit(r,1,true,true));Frame wrong;wrong.reg=4;p.response(wrong,true,{},1);assert(p.pending());
 Frame basic;basic.reg=3;basic.size=23;basic.payload[20]=3;p.response(basic,true,{},2);
 uint8_t wire[9];size_t n=0;assert(p.command(wire,n));const uint8_t expected[]={0xdd,0x5a,0xe1,2,0,1,0xff,0x1c,0x77};assert(!std::memcmp(wire,expected,9));
 Frame ack;ack.reg=0xe1;ack.status=0x80;p.response(ack,true,{},3);assert(!p.pending()&&!std::strcmp(p.result().reason,"device_rejected"));
 for(const char* why:{"disconnected","ota","write_admission_failed"}){assert(p.submit(r,4,true,true));p.cancel(why);assert(!p.pending()&&!std::strcmp(p.result().reason,why));}
 assert(p.submit(r,0,true,true));p.tick(30000);assert(!p.pending()&&!std::strcmp(p.result().reason,"expired"));
 r.deadline=10;assert(p.submit(r,0xfffffff0u,true,true));p.tick(9);assert(p.pending());p.tick(10);assert(!p.pending());
 r.deadline=30000;assert(p.submit(r,0,true,true));p.response(basic,false,{},1);assert(!p.pending());
 assert(p.submit(r,0,true,true));p.response(basic,true,{},1);ack.status=0;p.response(ack,true,{},2);p.response(basic,true,{},3);
 Snapshot mismatch;mismatch.available=true;mismatch.receivedAt=4;mismatch.values[8]={true,1};mismatch.values[9]={true,1};Frame cells;cells.reg=4;p.response(cells,true,mismatch,4);
 assert(!p.pending()&&!std::strcmp(p.result().reason,"readback_mismatch"));
 // Write errors retain explicit status, while corrupt checksums remain rejected.
 Parser parser;Frame decoded;const uint8_t error[]={0xdd,0xe1,0x80,0,0xff,0x80,0x77};bool parsed=false;
 for(auto b:error)parsed=parser.push(b,decoded)||parsed;assert(parsed&&decoded.status==0x80);
 std::puts("MOS absolute commands, preserved opposite permission, errors, expiry and readback passed");
}
