#pragma once
#include "JbdProtocol.h"
namespace bms {
enum class MosTarget { Charge, Discharge };
struct MosRequest { MosTarget target=MosTarget::Charge;bool state=false;char id[33]{};uint32_t deadline=0; };
struct MosResult {char id[33]{};const char* phase="idle";const char* reason="none";bool charge=false,discharge=false;uint32_t sequence=0;};
// Caller holds its mutex for offer/take/settle; taking never releases the in-flight reservation.
class MosMailbox {
 MosRequest request_{};bool queued_=false,reserved_=false;const char* reason_="none";
public:
 const char* offer(const MosRequest& request,uint32_t now,bool enabled,bool ready,bool cancelled){
  if(!enabled)return reason_="rejected_disabled";
  if(cancelled)return reason_="rejected_cancelled";
  if(reserved_)return reason_="rejected_busy";
  if(!ready)return reason_="rejected_unavailable";
  if(int32_t(now-request.deadline)>=0)return reason_="rejected_expired";
  request_=request;queued_=reserved_=true;return reason_="accepted";
 }
 bool take(MosRequest& request){if(!queued_)return false;request=request_;queued_=false;return true;}
 void settle(bool active){if(!active&&!queued_)reserved_=false;}
 void cancel(){if(reserved_)reason_="cancelled";queued_=reserved_=false;}
 bool pending()const{return reserved_;}
 const char* reason()const{return reason_;}
};
class BmsCommandPolicy {
 enum class Phase { Idle, PreRead, Write, ReadBasic, ReadCells };
 Phase phase_=Phase::Idle;MosRequest request_{};MosResult result_{};uint8_t desired_=0;uint32_t readAt_=0;
 void finish(const char* phase,const char* reason){phase_=Phase::Idle;result_.phase=phase;result_.reason=reason;++result_.sequence;}
public:
 bool pending()const{return phase_!=Phase::Idle;}
 const MosResult& result()const{return result_;}
 bool submit(const MosRequest& request,uint32_t now,bool enabled,bool ready){
  if(pending())return false;
  request_=request;std::memcpy(result_.id,request.id,sizeof(result_.id));result_.id[32]=0;
  if(!enabled){finish("rejected","disabled");return false;}
  if(!ready){finish("rejected","source_unavailable");return false;}
  if(int32_t(now-request.deadline)>=0){finish("rejected","expired");return false;}
  phase_=Phase::PreRead;result_.phase="pending";result_.reason="pre_read";++result_.sequence;return true;
 }
 void cancel(const char* reason){if(pending())finish("failed",reason);}
 void tick(uint32_t now){if(pending()&&int32_t(now-request_.deadline)>=0)cancel("expired");}
 uint8_t expected()const{return phase_==Phase::Write?0xe1:phase_==Phase::ReadCells?4:phase_==Phase::Idle?0:3;}
 bool command(uint8_t (&out)[9],size_t& size)const{
  if(!pending())return false;
  if(phase_!=Phase::Write){uint8_t read[7];if(!readCommand(expected(),read))return false;std::memcpy(out,read,7);size=7;return true;}
  const uint8_t disable=desired_^3;const uint16_t checksum=uint16_t(0-(0xe3+disable));
  const uint8_t bytes[]={0xdd,0x5a,0xe1,2,0,disable,uint8_t(checksum>>8),uint8_t(checksum),0x77};std::memcpy(out,bytes,9);size=9;return true;
 }
 void response(const Frame& frame,bool valid,const Snapshot& sample,uint32_t now){
  tick(now);if(!pending()||frame.reg!=expected())return;
  if(frame.status){cancel("device_rejected");return;}
  if(!valid){cancel("invalid_response");return;}
  if(phase_==Phase::PreRead){
   if(frame.size<23){cancel("invalid_response");return;}
   const uint8_t mask=request_.target==MosTarget::Charge?1:2;
   desired_=static_cast<uint8_t>((frame.payload[20]&3&~mask)|(request_.state?mask:0));phase_=Phase::Write;result_.reason="write";
  }else if(phase_==Phase::Write){
   if(frame.size){cancel("invalid_write_ack");return;}
   phase_=Phase::ReadBasic;readAt_=now;result_.reason="readback";
  }else if(phase_==Phase::ReadBasic){phase_=Phase::ReadCells;}
  else if(phase_==Phase::ReadCells){
   if(!sample.available||int32_t(sample.receivedAt-readAt_)<0||!sample.values[8].valid||!sample.values[9].valid){cancel("invalid_readback");return;}
   result_.charge=sample.values[8].number!=0;result_.discharge=sample.values[9].number!=0;
   // Verify both flags: preservation of the opposite permission is part of the command.
   const auto actual=(result_.charge?1:0)|(result_.discharge?2:0);
   finish(actual==desired_?"verified":"failed",actual==desired_?"ok":"readback_mismatch");return;
  }
  ++result_.sequence;
 }
};
}
