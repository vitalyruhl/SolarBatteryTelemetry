from pathlib import Path
import argparse,hashlib,json,re
ROOT=Path(__file__).resolve().parent
INPUTS=ROOT.parent/'upstream'
parser=argparse.ArgumentParser(description='Regenerate the recovered Windows host harness offline.')
parser.add_argument('--output-dir',type=Path,default=ROOT/'.build')
OUTPUT=parser.parse_args().output_dir.resolve()
input_manifest=json.loads((INPUTS/'source-inputs.json').read_text(encoding='utf-8'))
for entry in input_manifest['files']:
    actual=hashlib.sha256((INPUTS/entry['path']).read_bytes()).hexdigest()
    if actual!=entry['sha256']:
        raise RuntimeError('pinned input hash mismatch: '+entry['path'])
OUTPUT.mkdir(parents=True,exist_ok=True)

def function(src,name):
    # Extract a complete, unchanged definition. Start at its signature, not a call/declaration.
    pattern=re.compile(r'^(?:static\s+)?(?:inline\s+)?(?:[\w:*<>]+\s+)+'+re.escape(name)+r'\([^;\n]*\)\s*\{',re.M)
    match=pattern.search(src)
    if not match: raise RuntimeError('definition not found: '+name)
    start=match.start(); opening=src.index('{',match.start()); depth=1; i=opening+1
    # These selected functions contain balanced braces in comments/literals.
    while depth:
        depth += (src[i]=='{')-(src[i]=='}'); i+=1
    return src[start:i]

def section(src,start,end):
    a=src.index(start); b=src.index(end,a)+len(end); return src[a:b]

common=r'''
// [MOCKED!] Windows host model: FreeRTOS, TCP, callbacks, and task scheduling are stubs.
#include <windows.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
constexpr int ERR_OK=0, ERR_MEM=-1, ERR_CONN=-11;
using err_t=int; using esp_err_t=int;
constexpr unsigned portMAX_DELAY=0xffffffffU;
constexpr int pdPASS=1, pdFALSE=0;
struct tcp_pcb{}; struct pbuf{}; struct ip_addr{unsigned value;}; using ip_addr_t=ip_addr;
static bool fail_allocation=false;
static void* fault_malloc(size_t n){return fail_allocation ? nullptr : std::malloc(n);}
void* operator new(size_t n,const std::nothrow_t&) noexcept {if(fail_allocation)return nullptr;try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
static void pbuf_free(pbuf*){}
static void tcp_arg(tcp_pcb*,void*){}
static void tcp_sent(tcp_pcb*,void*){}
static void tcp_recv(tcp_pcb*,void*){}
static void tcp_err(tcp_pcb*,void*){}
static void tcp_poll(tcp_pcb*,void*,int){}
static int tcp_close(tcp_pcb*){return ERR_OK;}
static void tcp_abort(tcp_pcb*){}
static std::function<void()> ack_callback;
static std::mutex sync_mutex; static std::condition_variable sync_cv;
static bool callback_started=false, producer_finished=false, callback_completed=false;
static std::atomic<bool> send_blocked=false, cancel_wait=false;
static void on_ack(){
  {std::unique_lock<std::mutex> g(sync_mutex);callback_started=true;sync_cv.notify_all();sync_cv.wait(g,[]{return producer_finished;});}
}
class AsyncClient;
class AsyncServer{public: static void _s_accepted(void*,AsyncClient*){};void _accepted(AsyncClient*){};};
class AsyncClient{
public:
 tcp_pcb* _pcb=nullptr; int _closed_slot=0; size_t _rx_ack_len=0;
 void(*_discard_cb)(void*,AsyncClient*)=nullptr;void* _discard_cb_arg=nullptr;
 void close(OLD_CLOSE_SIGNATURE); int8_t _close();int8_t abort(){return ERR_OK;}
 static void _s_lwip_fin(void*,tcp_pcb*,int8_t){}
 static void _s_recv(void*,tcp_pcb*,pbuf*,int8_t){}
 static void _s_fin(void*,tcp_pcb*,int8_t){}
 static void _s_sent(void*,tcp_pcb*,uint16_t){ack_callback();}
 static void _s_poll(void*,tcp_pcb*){}
 static void _s_error(void*,int8_t){}
 static void _s_connected(void*,void*,int8_t){}
 static void _s_dns_found(const char*,ip_addr_t*,void*){}
 void _recv(tcp_pcb*,pbuf*,int8_t){}
 void _fin(tcp_pcb*,int8_t){}
 void _sent(tcp_pcb*,uint16_t){ack_callback();}
 void _poll(tcp_pcb*){} void _error(int8_t){} void _connected(tcp_pcb*,int8_t){} void _dns_found(ip_addr_t*){}
};
'''

old=(INPUTS/'esphome/AsyncTCP.cpp').read_text()
old_parts=[section(old,'typedef enum {','} lwip_event_packet_t;')]
old_shim=r'''
struct HostQueue{std::mutex mutex;std::condition_variable changed;std::deque<void*> items;size_t capacity;};
using xQueueHandle=HostQueue*;
static xQueueHandle _async_queue=nullptr;
static HostQueue* xQueueCreate(size_t cap,size_t){auto q=new HostQueue;q->capacity=cap;return q;}
static int send_queue(HostQueue* q,void* item,unsigned wait,bool front){
 std::unique_lock<std::mutex> g(q->mutex);
 if(q->items.size()==q->capacity){
  if(wait==0)return pdFALSE;
  assert(wait==portMAX_DELAY);send_blocked=true;sync_cv.notify_all();
  q->changed.wait(g,[q]{return q->items.size()<q->capacity||cancel_wait;});
  if(cancel_wait)return pdFALSE;
 }
 void* p=*static_cast<void**>(item);if(front)q->items.push_front(p);else q->items.push_back(p);q->changed.notify_all();return pdPASS;
}
static int xQueueSend(HostQueue*q,void*p,unsigned t){return send_queue(q,p,t,false);}
static int xQueueSendToFront(HostQueue*q,void*p,unsigned t){return send_queue(q,p,t,true);}
static int xQueueReceive(HostQueue*q,void*p,unsigned wait){
 std::unique_lock<std::mutex>g(q->mutex);if(q->items.empty()){if(!wait)return pdFALSE;q->changed.wait(g,[q]{return !q->items.empty();});}
 *static_cast<void**>(p)=q->items.front();q->items.pop_front();q->changed.notify_all();return pdPASS;
}
static int xQueuePeek(HostQueue*q,void*p,unsigned){std::lock_guard<std::mutex>g(q->mutex);if(q->items.empty())return pdFALSE;*static_cast<void**>(p)=q->items.front();return pdPASS;}
static void _tcp_recved(tcp_pcb*,int,size_t){}
static int8_t _tcp_close(tcp_pcb*,int){return ERR_OK;}
#define malloc fault_malloc
'''
old_names=['_init_async_event_queue','_send_async_event','_prepend_async_event','_get_async_event','_remove_events_with_arg','_handle_async_event','_tcp_clear_events','_tcp_connected','_tcp_poll','_tcp_recv','_tcp_sent','_tcp_error','_tcp_dns_found','_tcp_accept','AsyncClient::close','AsyncClient::_close']
old_generated=common.replace('OLD_CLOSE_SIGNATURE','bool now=false')+'\n'+old_parts[0]+'\n'+old_shim+'\n'+'\n\n'.join(function(old,n) for n in old_names)+'\n#undef malloc\n'
old_driver=r'''
static size_t queue_size(){std::lock_guard<std::mutex>g(_async_queue->mutex);return _async_queue->items.size();}
int main(int argc,char**argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 std::string mode=argc>1?argv[1]:"";_init_async_event_queue();tcp_pcb pcb;pbuf pb;AsyncClient target,other;target._pcb=&pcb;
 if(mode.rfind("oom-",0)==0){fail_allocation=true;std::cerr<<"injecting allocation failure in "<<mode<<std::endl;
  if(mode=="oom-poll")_tcp_poll(&target,&pcb);
  else if(mode=="oom-recv")_tcp_recv(&target,&pcb,&pb,ERR_OK);
  else if(mode=="oom-sent")_tcp_sent(&target,&pcb,1);
  else if(mode=="oom-error")_tcp_error(&target,ERR_MEM);
  else if(mode=="oom-clear")_tcp_clear_events(&target);
  else if(mode=="oom-connected")_tcp_connected(&target,&pcb,ERR_OK);
  else if(mode=="oom-dns")_tcp_dns_found("example.invalid",nullptr,&target);
  else if(mode=="oom-accept")_tcp_accept(&target,&other);
  else return 3;
  std::cout<<"unexpected allocation survival\n";return 4;
 }
 bool refill=mode=="queue-refill";
 _tcp_sent(&target,&pcb,1);for(int i=0;i<31;++i)_tcp_poll(&other,&pcb);
 assert(queue_size()==32);
 ack_callback=[&]{on_ack();target.close();{std::lock_guard<std::mutex>g(sync_mutex);callback_completed=true;}sync_cv.notify_all();};
 std::thread consumer([&]{lwip_event_packet_t* e=nullptr;_get_async_event(&e);_handle_async_event(e);});
 {std::unique_lock<std::mutex>g(sync_mutex);if(!sync_cv.wait_for(g,std::chrono::seconds(2),[]{return callback_started;}))std::terminate();}
 if(refill)_tcp_poll(&other,&pcb);
 {std::lock_guard<std::mutex>g(sync_mutex);producer_finished=true;}sync_cv.notify_all();
 {std::unique_lock<std::mutex>g(sync_mutex);if(!sync_cv.wait_for(g,std::chrono::seconds(2),[]{return callback_completed||send_blocked.load();}))std::terminate();}
 bool blocked=send_blocked.load(), completed=callback_completed;size_t size=queue_size();
 cancel_wait=true;_async_queue->changed.notify_all();consumer.join();
 std::cout<<"{\"blocked\":"<<(blocked?"true":"false")<<",\"completed_before_release\":"<<(completed?"true":"false")<<",\"queue_size\":"<<size<<"}\n";
 while(!_async_queue->items.empty()){std::free(_async_queue->items.front());_async_queue->items.pop_front();}delete _async_queue;
 return (blocked==refill && completed!=refill && size==32)?0:5;
}
'''
(OUTPUT/'old_harness.cpp').write_text(old_generated+old_driver)

stable=(INPUTS/'esp32async-v3.5.0/AsyncTCP.cpp').read_text()
stable_type=section(stable,'typedef enum {','inline lwip_tcp_event_packet_t(lwip_tcp_event_t _event, AsyncClient *_client) : next(nullptr), event(_event), client(_client){};')+'\n};\n'
stable_shim=r'''
#define async_tcp_log_e(...) ((void)0)
#define async_tcp_log_d(...) ((void)0)
#define async_tcp_log_elapsed(tag,statement) statement
#define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
using SemaphoreHandle_t=std::mutex*;
static std::mutex host_queue_mutex;
static SemaphoreHandle_t _async_queue_mutex=&host_queue_mutex;
static int xSemaphoreTake(SemaphoreHandle_t m,unsigned){m->lock();return 1;}
static void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
static void* _async_service_task_handle=nullptr;
static void xTaskNotifyGive(void*){}
#include "AsyncTCPSimpleIntrusiveList.h"
static SimpleIntrusiveList<lwip_tcp_event_packet_t> _async_queue;
class queue_mutex_guard{
 bool holds_mutex;
public:
 inline queue_mutex_guard():holds_mutex(xSemaphoreTake(_async_queue_mutex,portMAX_DELAY)){}
 inline ~queue_mutex_guard(){if(holds_mutex)xSemaphoreGive(_async_queue_mutex);}
 inline explicit operator bool() const{return holds_mutex;}
};
class AsyncTCP_detail{
public:
 static void handle_async_event(lwip_tcp_event_packet_t*);
 static int8_t tcp_poll(void*,tcp_pcb*);
 static int8_t tcp_recv(void*,tcp_pcb*,pbuf*,int8_t);
 static int8_t tcp_sent(void*,tcp_pcb*,uint16_t);
 static void tcp_error(void*,int8_t);
};
static uint32_t _xor_shift_state=31;
struct tcpip_api_call_data{};
struct tcp_api_call_t:tcpip_api_call_data {tcp_pcb** pcb;AsyncClient* close;err_t err;};
static void tcpip_api_call(err_t(*fn)(tcpip_api_call_data*),tcpip_api_call_data* msg){fn(msg);}
static void _tcp_recved(tcp_pcb**,size_t){}
'''
stable_names=['_xor_shift_next','_free_event','_send_async_event','_prepend_async_event','_remove_events_for_client','AsyncTCP_detail::handle_async_event','_reset_tcp_callbacks','_tcp_connected','AsyncTCP_detail::tcp_poll','AsyncTCP_detail::tcp_recv','AsyncTCP_detail::tcp_sent','AsyncTCP_detail::tcp_error','_tcp_dns_found','_tcp_close_api','_tcp_close','AsyncClient::close','AsyncClient::_close']
stable_generated=common.replace('OLD_CLOSE_SIGNATURE','')+'\n'+stable_type+'\n'+stable_shim+'\n'+'\n\n'.join(function(stable,n) for n in stable_names)
stable_driver=r'''
static size_t queue_size(){queue_mutex_guard g;return _async_queue.size();}
int main(int argc,char**argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 std::string mode=argc>1?argv[1]:"";tcp_pcb pcb;pbuf pb;AsyncClient target,other;target._pcb=&pcb;
 if(mode.rfind("oom-",0)==0){fail_allocation=true;int ret=ERR_MEM;
  if(mode=="oom-poll")ret=AsyncTCP_detail::tcp_poll(&target,&pcb);
  else if(mode=="oom-recv")ret=AsyncTCP_detail::tcp_recv(&target,&pcb,&pb,ERR_OK);
  else if(mode=="oom-sent")ret=AsyncTCP_detail::tcp_sent(&target,&pcb,1);
  else if(mode=="oom-error")AsyncTCP_detail::tcp_error(&target,ERR_MEM);
  else if(mode=="oom-connected")ret=_tcp_connected(&target,&pcb,ERR_OK);
  else if(mode=="oom-dns")_tcp_dns_found("example.invalid",nullptr,&target);
  else return 3;
  std::cout<<"{\"allocation_failure_handled\":"<<((ret==ERR_MEM&&queue_size()==0)?"true":"false")<<",\"queue_size\":"<<queue_size()<<"}\n";
  return ret==ERR_MEM&&queue_size()==0?0:4;
 }
 bool refill=mode=="queue-refill";
 AsyncTCP_detail::tcp_sent(&target,&pcb,1);for(int i=0;i<31;++i)AsyncTCP_detail::tcp_sent(&other,&pcb,1);
 assert(queue_size()==32);
 ack_callback=[&]{on_ack();target.close();{std::lock_guard<std::mutex>g(sync_mutex);callback_completed=true;}sync_cv.notify_all();};
 std::thread consumer([&]{lwip_tcp_event_packet_t*e;{queue_mutex_guard g;e=_async_queue.pop_front();}AsyncTCP_detail::handle_async_event(e);});
 {std::unique_lock<std::mutex>g(sync_mutex);if(!sync_cv.wait_for(g,std::chrono::seconds(2),[]{return callback_started;}))std::terminate();}
 if(refill)AsyncTCP_detail::tcp_sent(&other,&pcb,1);
 {std::lock_guard<std::mutex>g(sync_mutex);producer_finished=true;}sync_cv.notify_all();
 {std::unique_lock<std::mutex>g(sync_mutex);if(!sync_cv.wait_for(g,std::chrono::seconds(2),[]{return callback_completed;}))std::terminate();}
 consumer.join();size_t size=queue_size();
 std::cout<<"{\"blocked\":false,\"completed_before_release\":true,\"queue_size\":"<<size<<"}\n";
 return size==(refill?32:31)?0:5;
}
'''
(OUTPUT/'stable_harness.cpp').write_text(stable_generated+'\n'+stable_driver)
manifest={'extraction':'Selected complete function bodies copied unchanged by signature from pinned upstream source. Platform, TCP transport, application callbacks, and task scheduling are host stubs. No source fixes are applied.','sources':{},'symbols':{'old':old_names,'stable':stable_names}}
for name,relative in [('esphome','esphome/AsyncTCP.cpp'),('esp32async-stable','esp32async-v3.5.0/AsyncTCP.cpp')]:
    entry=next(item for item in input_manifest['files'] if item['path']==relative)
    manifest['sources'][name]={'commit':entry['commit'],'sha256':entry['sha256'],'upstream_git_blob':entry['upstream_git_blob']}
manifest['scope']='Recovered offline harness; exact pinned source bodies with explicit host stubs. No firmware or hardware test.'
(OUTPUT/'extraction-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('Generated host harnesses in',OUTPUT)
