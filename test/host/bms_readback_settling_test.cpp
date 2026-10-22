#include "bms/BmsCommandPolicy.h"
#include <cassert>
#include <cstdio>
using namespace bms;

// [MOCKED!] The owner-observed zero-current sequence followed by conducting FETs.
static MosRequest request(MosTarget target,bool state,uint32_t deadline=30000) {
 MosRequest value;value.target=target;value.state=state;value.deadline=deadline;return value;
}
static Frame basic(uint8_t flags=3) {
 Frame frame;frame.reg=3;frame.size=23;frame.payload[20]=flags;return frame;
}
static void write(BmsCommandPolicy& policy,const MosRequest& value,uint8_t disabled,uint32_t now=0,uint8_t actual=3) {
 assert(policy.submit(value,now,true,true));policy.response(basic(actual),true,{},now+1);
 uint8_t bytes[9];size_t size=0;assert(policy.command(bytes,size)&&size==9&&bytes[5]==disabled);
 policy.markWriteSent();
}
static void ack(BmsCommandPolicy& policy,uint32_t now=2) {
 Frame frame;frame.reg=0xe1;policy.response(frame,true,{},now);
 assert(!policy.pending()&&policy.permissionsKnown()&&!std::strcmp(policy.result().phase,"acknowledged"));
}
int main() {
 BmsCommandPolicy policy;
 auto initialize=request(MosTarget::Both,true);initialize.dischargeState=true;
 write(policy,initialize,0);ack(policy);assert(policy.permissionMask()==3);
 write(policy,request(MosTarget::Charge,false),1,10);ack(policy,12);assert(policy.permissionMask()==2);
 write(policy,request(MosTarget::Discharge,false),3,20,2);ack(policy,22);assert(policy.permissionMask()==0);
 write(policy,request(MosTarget::Charge,true),2,30,0);ack(policy,32);assert(policy.permissionMask()==1);
 // Fresh actual FETs may both conduct while charging without changing the acknowledged lock.
 Snapshot charging;charging.available=true;charging.receivedAt=1000;charging.sequence=1;
 charging.values[4]={true,12.53};charging.values[8]={true,1};charging.values[9]={true,1};
 policy.response(basic(3),true,charging,1000);Frame cells;cells.reg=4;policy.response(cells,true,charging,1050);
 policy.tick(30000);assert(policy.permissionsKnown()&&policy.permissionMask()==1);
 assert(!policy.pending()&&!std::strcmp(policy.result().phase,"acknowledged"));
 // The next explicit charge operation uses the newly read actual opposite FET state.
 write(policy,request(MosTarget::Charge,false,60000),1,31000,3);ack(policy,31002);assert(policy.permissionMask()==2);

 BmsCommandPolicy unknown;assert(!unknown.permissionsKnown());
 assert(unknown.submit(request(MosTarget::Discharge,false),0,true,true));unknown.cancel("cancelled");
 // Expiry before a write preserves the old request; after queueing it leaves the state unknown.
 BmsCommandPolicy before;write(before,initialize,0);ack(before);
 assert(before.submit(request(MosTarget::Charge,false,20),10,true,true));before.tick(20);
 assert(!before.pending()&&before.permissionsKnown()&&before.permissionMask()==3);
 BmsCommandPolicy sent;write(sent,initialize,0);ack(sent);
 write(sent,request(MosTarget::Charge,false,20),1,10);sent.tick(20);
 assert(!sent.pending()&&!sent.permissionsKnown()&&!std::strcmp(sent.result().reason,"expired"));

 // Rollover-safe expiry still distinguishes before/after a potentially executed write.
 BmsCommandPolicy rollover;initialize.deadline=10;write(rollover,initialize,0,0xfffffff0u);
 rollover.tick(9);assert(rollover.pending());ack(rollover,9);assert(rollover.permissionMask()==3);
 auto expired=request(MosTarget::Discharge,false,30);
 write(rollover,expired,2,15);rollover.tick(30);assert(!rollover.permissionsKnown());
 // A late ACK after cancellation cannot restore an unconfirmed software permission.
 Frame late;late.reg=0xe1;rollover.response(late,true,{},31);assert(!rollover.permissionsKnown());
 std::puts("MOS conducting-FET independence, controlled sequence, deadline and rollover checks passed");
}
