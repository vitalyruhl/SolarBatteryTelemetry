
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
 void close(bool now=false); int8_t _close();int8_t abort(){return ERR_OK;}
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

typedef enum {
    LWIP_TCP_SENT, LWIP_TCP_RECV, LWIP_TCP_FIN, LWIP_TCP_ERROR, LWIP_TCP_POLL, LWIP_TCP_CLEAR, LWIP_TCP_ACCEPT, LWIP_TCP_CONNECTED, LWIP_TCP_DNS
} lwip_event_t;

typedef struct {
        lwip_event_t event;
        void *arg;
        union {
                struct {
                        void * pcb;
                        int8_t err;
                } connected;
                struct {
                        int8_t err;
                } error;
                struct {
                        tcp_pcb * pcb;
                        uint16_t len;
                } sent;
                struct {
                        tcp_pcb * pcb;
                        pbuf * pb;
                        int8_t err;
                } recv;
                struct {
                        tcp_pcb * pcb;
                        int8_t err;
                } fin;
                struct {
                        tcp_pcb * pcb;
                } poll;
                struct {
                        AsyncClient * client;
                } accept;
                struct {
                        const char * name;
                        ip_addr_t addr;
                } dns;
        };
} lwip_event_packet_t;

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

static inline bool _init_async_event_queue(){
    if(!_async_queue){
        _async_queue = xQueueCreate(32, sizeof(lwip_event_packet_t *));
        if(!_async_queue){
            return false;
        }
    }
    return true;
}

static inline bool _send_async_event(lwip_event_packet_t ** e){
    return _async_queue && xQueueSend(_async_queue, e, portMAX_DELAY) == pdPASS;
}

static inline bool _prepend_async_event(lwip_event_packet_t ** e){
    return _async_queue && xQueueSendToFront(_async_queue, e, portMAX_DELAY) == pdPASS;
}

static inline bool _get_async_event(lwip_event_packet_t ** e){
    return _async_queue && xQueueReceive(_async_queue, e, portMAX_DELAY) == pdPASS;
}

static bool _remove_events_with_arg(void * arg){
    lwip_event_packet_t * first_packet = NULL;
    lwip_event_packet_t * packet = NULL;

    if(!_async_queue){
        return false;
    }
    //figure out which is the first packet so we can keep the order
    while(!first_packet){
        if(xQueueReceive(_async_queue, &first_packet, 0) != pdPASS){
            return false;
        }
        //discard packet if matching
        if((int)first_packet->arg == (int)arg){
            free(first_packet);
            first_packet = NULL;
        //return first packet to the back of the queue
        } else if(xQueueSend(_async_queue, &first_packet, portMAX_DELAY) != pdPASS){
            return false;
        }
    }

    while(xQueuePeek(_async_queue, &packet, 0) == pdPASS && packet != first_packet){
        if(xQueueReceive(_async_queue, &packet, 0) != pdPASS){
            return false;
        }
        if((int)packet->arg == (int)arg){
            free(packet);
            packet = NULL;
        } else if(xQueueSend(_async_queue, &packet, portMAX_DELAY) != pdPASS){
            return false;
        }
    }
    return true;
}

static void _handle_async_event(lwip_event_packet_t * e){
    if(e->arg == NULL){
        // do nothing when arg is NULL
        //ets_printf("event arg == NULL: 0x%08x\n", e->recv.pcb);
    } else if(e->event == LWIP_TCP_CLEAR){
        _remove_events_with_arg(e->arg);
    } else if(e->event == LWIP_TCP_RECV){
        //ets_printf("-R: 0x%08x\n", e->recv.pcb);
        AsyncClient::_s_recv(e->arg, e->recv.pcb, e->recv.pb, e->recv.err);
    } else if(e->event == LWIP_TCP_FIN){
        //ets_printf("-F: 0x%08x\n", e->fin.pcb);
        AsyncClient::_s_fin(e->arg, e->fin.pcb, e->fin.err);
    } else if(e->event == LWIP_TCP_SENT){
        //ets_printf("-S: 0x%08x\n", e->sent.pcb);
        AsyncClient::_s_sent(e->arg, e->sent.pcb, e->sent.len);
    } else if(e->event == LWIP_TCP_POLL){
        //ets_printf("-P: 0x%08x\n", e->poll.pcb);
        AsyncClient::_s_poll(e->arg, e->poll.pcb);
    } else if(e->event == LWIP_TCP_ERROR){
        //ets_printf("-E: 0x%08x %d\n", e->arg, e->error.err);
        AsyncClient::_s_error(e->arg, e->error.err);
    } else if(e->event == LWIP_TCP_CONNECTED){
        //ets_printf("C: 0x%08x 0x%08x %d\n", e->arg, e->connected.pcb, e->connected.err);
        AsyncClient::_s_connected(e->arg, e->connected.pcb, e->connected.err);
    } else if(e->event == LWIP_TCP_ACCEPT){
        //ets_printf("A: 0x%08x 0x%08x\n", e->arg, e->accept.client);
        AsyncServer::_s_accepted(e->arg, e->accept.client);
    } else if(e->event == LWIP_TCP_DNS){
        //ets_printf("D: 0x%08x %s = %s\n", e->arg, e->dns.name, ipaddr_ntoa(&e->dns.addr));
        AsyncClient::_s_dns_found(e->dns.name, &e->dns.addr, e->arg);
    }
    free((void*)(e));
}

static int8_t _tcp_clear_events(void * arg) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_CLEAR;
    e->arg = arg;
    if (!_prepend_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_connected(void * arg, tcp_pcb * pcb, int8_t err) {
    //ets_printf("+C: 0x%08x\n", pcb);
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_CONNECTED;
    e->arg = arg;
    e->connected.pcb = pcb;
    e->connected.err = err;
    if (!_prepend_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_poll(void * arg, struct tcp_pcb * pcb) {
    //ets_printf("+P: 0x%08x\n", pcb);
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_POLL;
    e->arg = arg;
    e->poll.pcb = pcb;
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_recv(void * arg, struct tcp_pcb * pcb, struct pbuf *pb, int8_t err) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->arg = arg;
    if(pb){
        //ets_printf("+R: 0x%08x\n", pcb);
        e->event = LWIP_TCP_RECV;
        e->recv.pcb = pcb;
        e->recv.pb = pb;
        e->recv.err = err;
    } else {
        //ets_printf("+F: 0x%08x\n", pcb);
        e->event = LWIP_TCP_FIN;
        e->fin.pcb = pcb;
        e->fin.err = err;
        //close the PCB in LwIP thread
        AsyncClient::_s_lwip_fin(e->arg, e->fin.pcb, e->fin.err);
    }
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_sent(void * arg, struct tcp_pcb * pcb, uint16_t len) {
    //ets_printf("+S: 0x%08x\n", pcb);
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_SENT;
    e->arg = arg;
    e->sent.pcb = pcb;
    e->sent.len = len;
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static void _tcp_error(void * arg, int8_t err) {
    //ets_printf("+E: 0x%08x\n", arg);
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_ERROR;
    e->arg = arg;
    e->error.err = err;
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
}

static void _tcp_dns_found(const char * name, struct ip_addr * ipaddr, void * arg) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    //ets_printf("+DNS: name=%s ipaddr=0x%08x arg=%x\n", name, ipaddr, arg);
    e->event = LWIP_TCP_DNS;
    e->arg = arg;
    e->dns.name = name;
    if (ipaddr) {
        memcpy(&e->dns.addr, ipaddr, sizeof(struct ip_addr));
    } else {
        memset(&e->dns.addr, 0, sizeof(e->dns.addr));
    }
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
}

static int8_t _tcp_accept(void * arg, AsyncClient * client) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_ACCEPT;
    e->arg = arg;
    e->accept.client = client;
    if (!_prepend_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

void AsyncClient::close(bool now){
    if(_pcb){
        _tcp_recved(_pcb, _closed_slot, _rx_ack_len);
    }
    _close();
}

int8_t AsyncClient::_close(){
    //ets_printf("X: 0x%08x\n", (uint32_t)this);
    int8_t err = ERR_OK;
    if(_pcb) {
        //log_i("");
        tcp_arg(_pcb, NULL);
        tcp_sent(_pcb, NULL);
        tcp_recv(_pcb, NULL);
        tcp_err(_pcb, NULL);
        tcp_poll(_pcb, NULL, 0);
        _tcp_clear_events(this);
        err = _tcp_close(_pcb, _closed_slot);
        if(err != ERR_OK) {
            err = abort();
        }
        _pcb = NULL;
        if(_discard_cb) {
            _discard_cb(_discard_cb_arg, this);
        }
    }
    return err;
}
#undef malloc

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
