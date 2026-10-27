#pragma once
// [MOCKED!] Synthetic ELF fixture shared by parser and SDK boundary tests.
#include <array>
#include <cstring>
#include <cstdint>
struct Reader {
 std::array<unsigned char,512> bytes{};unsigned reads=0;bool fail=false;
 bool read(uint32_t offset,void* out,std::size_t n){++reads;if(fail||offset>bytes.size()||n>bytes.size()-offset)return false;std::memcpy(out,bytes.data()+offset,n);return true;}
 void u16(unsigned at,uint16_t v){bytes[at]=v&255;bytes[at+1]=v>>8;}
 void u32(unsigned at,uint32_t v){for(unsigned i=0;i<4;++i)bytes[at+i]=(v>>(8*i))&255;}
 Reader(){u32(0,512);u32(4,0x100);bytes[20]=127;bytes[21]='E';bytes[22]='L';bytes[23]='F';bytes[24]=1;bytes[25]=1;bytes[26]=1;u16(36,4);u16(38,94);u32(40,1);u32(48,52);u16(60,52);u16(62,32);u16(64,1);ph(72,1,128,0x3ffb1000,100,100);text("abort() was called at PC 0x4011b064 on core 0");}
 void ph(unsigned at,uint32_t type,uint32_t offset,uint32_t address,uint32_t size,uint32_t mem){u32(at,type);u32(at+4,offset);u32(at+8,address);u32(at+16,size);u32(at+20,mem);}
 void text(const char* s){std::memset(bytes.data()+148,0,100);std::memcpy(bytes.data()+148,s,std::strlen(s));}
};
