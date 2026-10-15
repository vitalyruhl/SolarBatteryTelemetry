#include "victron/VictronDecoder.h"
#include "victron/VictronConfigMailbox.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <windows.h>
#include <bcrypt.h>

// [MOCKED!] Public synthetic fixture, never a device key or captured advertisement.
static bool hostDecrypt(const uint8_t* key,const uint8_t* nonce,const uint8_t* input,std::size_t size,uint8_t* output) {
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_KEY_HANDLE handle=nullptr;
    assert(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)==0);
    assert(BCryptSetProperty(algorithm,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),sizeof(BCRYPT_CHAIN_MODE_ECB),0)==0);
    assert(BCryptGenerateSymmetricKey(algorithm,&handle,nullptr,0,const_cast<PUCHAR>(key),16,0)==0);
    uint8_t stream[16]{};ULONG written=0;
    assert(BCryptEncrypt(handle,const_cast<PUCHAR>(nonce),16,nullptr,nullptr,0,stream,16,&written,0)==0);
    assert(written==16&&size<=16);
    for(std::size_t i=0;i<size;++i)output[i]=input[i]^stream[i];
    BCryptDestroyKey(handle);BCryptCloseAlgorithmProvider(algorithm,0);return true;
}
static bool plain(const uint8_t*,const uint8_t*,const uint8_t* input,std::size_t n,uint8_t* out){std::memcpy(out,input,n);return true;}
static bool cryptoFailure(const uint8_t*,const uint8_t*,const uint8_t*,std::size_t,uint8_t*){return false;}
static void expectNear(double a,double b){assert(std::fabs(a-b)<0.00001);}
int main(){
    using namespace victron;
    uint8_t key[16]{};assert(parseKey("000102030405060708090a0b0c0d0e0f",32,key));
    assert(!parseKey("000102030405060708090a0b0c0d0e0g",32,key));
    assert(!parseKey("000102030405060708090a0b0c0d0e0f",31,key));
    assert(parseKey("000102030405060708090a0b0c0d0e0f",32,key));
    assert(validMac("C0:01:02:03:04:05",17));assert(!validMac("C0-01-02-03-04-05",17));
    assert(validMac("C001aB03cD05",12));assert(!validMac("C001aB03cD0G",12));
    assert(!validMac("C001aB03cD0",11));assert(!validMac("C001aB03cD050",13));
    assert(!validMac("C0:01:02:03:04:0G",17));assert(!validMac("C0:01:02:03:04:05x",18));
    uint8_t frame[]={0xe1,0x02,0x10,0x02,0x58,0xa0,0x01,0x34,0x12,0x00,0x2a,0x60,0x81,0x19,0xdc,0x01,0x45,0xca,0x2e,0xe0,0x97,0xdf};
    Reading reading{};assert(decode(frame,sizeof(frame),key,hostDecrypt,reading)==DecodeResult::Ok);
    assert(reading.counter==0x1234&&reading.productId==0xa058);
    expectNear(reading.values[15].number,128);expectNear(reading.values[18].number,48.07);expectNear(reading.values[19].number,2.6);
    expectNear(reading.values[21].number,1.2);expectNear(reading.values[24].number,770);assert(reading.values[22].number==2);
    assert(!reading.values[16].valid&&!reading.values[17].valid&&!reading.values[20].valid);
    for(std::size_t n=0;n<sizeof(frame);++n)assert(decode(frame,n,key,hostDecrypt,reading)!=DecodeResult::Ok);
    assert(decode(frame,sizeof(frame),key,cryptoFailure,reading)==DecodeResult::CryptoError);
    uint8_t extended[30]{};std::memcpy(extended,frame,sizeof(frame));
    assert(decode(extended,sizeof(extended),key,hostDecrypt,reading)==DecodeResult::Ok);
    frame[9]=1;assert(decode(frame,sizeof(frame),key,hostDecrypt,reading)==DecodeResult::KeyMismatch);frame[9]=0;
    frame[6]=2;assert(decode(frame,sizeof(frame),key,hostDecrypt,reading)==DecodeResult::Unsupported);frame[6]=1;
    frame[0]=0;assert(decode(frame,sizeof(frame),key,hostDecrypt,reading)==DecodeResult::Ignored);frame[0]=0xe1;
    uint8_t unavailable[]={0xe1,2,0x10,2,0x58,0xa0,1,1,0,0,0xff,0xff,0xff,0x7f,0xff,0x7f,0xff,0xff,0xff,0xff,0xff,0xff};
    assert(decode(unavailable,sizeof(unavailable),key,plain,reading)==DecodeResult::Ok);
    for(const auto& value:reading.values)assert(!value.valid);
    unavailable[10]=247;unavailable[14]=0xf6;unavailable[15]=0xff;
    assert(decode(unavailable,sizeof(unavailable),key,plain,reading)==DecodeResult::Ok);
    assert(reading.values[22].number==6);expectNear(reading.values[19].number,-1);
    Receiver receiver;receiver.reset("waiting_for_advertisement");
    assert(receiver.receive(frame,sizeof(frame),key,100,-55,hostDecrypt));
    assert(receiver.snapshot(30099).available);assert(!receiver.snapshot(30100).available);
    assert(!receiver.receive(frame,sizeof(frame),key,30101,-55,hostDecrypt));assert(!receiver.snapshot(30101).available);
    frame[7]++;assert(receiver.receive(frame,sizeof(frame),key,0xfffffff0u,-55,hostDecrypt));
    assert(receiver.snapshot(0x10).available);assert(!receiver.snapshot(0x8000).available);
    frame[9]=1;receiver.receive(frame,sizeof(frame),key,200,-55,hostDecrypt);assert(!receiver.snapshot(200).available);
    frame[9]=0;frame[7]++;assert(!receiver.receive(frame,sizeof(frame),key,201,-55,hostDecrypt));
    receiver.reset("waiting_for_advertisement");assert(receiver.receive(frame,sizeof(frame),key,202,-55,hostDecrypt));
    ConfigMailbox mailbox;Configuration c;uint32_t seen=0;
    assert(mailbox.setMac("C0:01:02:03:04:05",17));assert(mailbox.setKey("000102030405060708090a0b0c0d0e0f",32));
    assert(mailbox.take(seen,c)&&c.ready());assert(!mailbox.take(seen,c));
    assert(mailbox.setMac("C001aB03cD05",12));assert(mailbox.take(seen,c)&&c.ready());
    assert(std::strcmp(c.mac,"c0:01:ab:03:cd:05")==0);
    assert(mailbox.setMac("C0:01:AB:03:CD:05",17));assert(mailbox.take(seen,c)&&c.ready());
    assert(std::strcmp(c.mac,"c0:01:ab:03:cd:05")==0);
    assert(!mailbox.setKey("bad",3));assert(mailbox.take(seen,c)&&!c.ready());
    assert(!mailbox.setMac("C0:01:02:03:04:05x",18));assert(mailbox.take(seen,c)&&!c.ready());
    assert(mailbox.setKey("",0));assert(mailbox.take(seen,c)&&!c.ready());
    std::puts("Victron decoder, AES fixture, validation and freshness passed");
}
