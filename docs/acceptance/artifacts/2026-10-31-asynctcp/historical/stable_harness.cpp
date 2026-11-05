
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
 void close(); int8_t _close();int8_t abort(){return ERR_OK;}
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
  LWIP_TCP_SENT,
  LWIP_TCP_RECV,
  LWIP_TCP_FIN,
  LWIP_TCP_ERROR,
  LWIP_TCP_POLL,
  LWIP_TCP_ACCEPT,
  LWIP_TCP_CONNECTED,
  LWIP_TCP_DNS
} lwip_tcp_event_t;

struct lwip_tcp_event_packet_t {
  lwip_tcp_event_packet_t *next;
  lwip_tcp_event_t event;
  AsyncClient *client;
  union {
    struct {
      tcp_pcb *pcb;
      int8_t err;
    } connected;
    struct {
      int8_t err;
    } error;
    struct {
      tcp_pcb *pcb;
      uint16_t len;
    } sent;
    struct {
      tcp_pcb *pcb;
      pbuf *pb;
      int8_t err;
    } recv;
    struct {
      tcp_pcb *pcb;
      int8_t err;
    } fin;
    struct {
      tcp_pcb *pcb;
    } poll;
    struct {
      AsyncServer *server;
    } accept;
    struct {
      const char *name;
      ip_addr_t addr;
    } dns;
  };

  inline lwip_tcp_event_packet_t(lwip_tcp_event_t _event, AsyncClient *_client) : next(nullptr), event(_event), client(_client){};
};


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
#include "esp32async-stable/src/AsyncTCPSimpleIntrusiveList.h"
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

static uint32_t _xor_shift_next() {
  uint32_t x = _xor_shift_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return _xor_shift_state = x;
}

static void _free_event(lwip_tcp_event_packet_t *evpkt) {
  if ((evpkt->event == LWIP_TCP_RECV) && (evpkt->recv.pb != nullptr)) {
    pbuf_free(evpkt->recv.pb);
  }
  delete evpkt;
}

static inline void _send_async_event(lwip_tcp_event_packet_t *e) {
  if (e == nullptr) {
    return;
  }
  _async_queue.push_back(e);
  xTaskNotifyGive(_async_service_task_handle);
}

static inline void _prepend_async_event(lwip_tcp_event_packet_t *e) {
  if (e == nullptr) {
    return;
  }
  _async_queue.push_front(e);
  xTaskNotifyGive(_async_service_task_handle);
}

static size_t _remove_events_for_client(AsyncClient *client) {
  lwip_tcp_event_packet_t *removed_event_chain;
  {
    queue_mutex_guard guard;
    removed_event_chain = _async_queue.remove_if([=](lwip_tcp_event_packet_t &pkt) {
      return pkt.client == client;
    });
  }

  size_t count = 0;
  while (removed_event_chain) {
    ++count;
    auto t = removed_event_chain;
    removed_event_chain = t->next;
    _free_event(t);
  }
  return count;
}

void AsyncTCP_detail::handle_async_event(lwip_tcp_event_packet_t *e) {
  if (e->client == NULL) {
    // do nothing when arg is NULL
    // ets_printf("event arg == NULL: 0x%08x\n", e->recv.pcb);
  } else if (e->event == LWIP_TCP_RECV) {
    // ets_printf("-R: 0x%08x\n", e->recv.pcb);
    e->client->_recv(e->recv.pcb, e->recv.pb, e->recv.err);
    e->recv.pb = nullptr;  // given to client
  } else if (e->event == LWIP_TCP_FIN) {
    // ets_printf("-F: 0x%08x\n", e->fin.pcb);
    e->client->_fin(e->fin.pcb, e->fin.err);
  } else if (e->event == LWIP_TCP_SENT) {
    // ets_printf("-S: 0x%08x\n", e->sent.pcb);
    e->client->_sent(e->sent.pcb, e->sent.len);
  } else if (e->event == LWIP_TCP_POLL) {
    // ets_printf("-P: 0x%08x\n", e->poll.pcb);
    e->client->_poll(e->poll.pcb);
  } else if (e->event == LWIP_TCP_ERROR) {
    // ets_printf("-E: 0x%08x %d\n", e->client, e->error.err);
    e->client->_error(e->error.err);
  } else if (e->event == LWIP_TCP_CONNECTED) {
    // ets_printf("C: 0x%08x 0x%08x %d\n", e->client, e->connected.pcb, e->connected.err);
    e->client->_connected(e->connected.pcb, e->connected.err);
  } else if (e->event == LWIP_TCP_ACCEPT) {
    // ets_printf("A: 0x%08x 0x%08x\n", e->client, e->accept.client);
    e->accept.server->_accepted(e->client);
  } else if (e->event == LWIP_TCP_DNS) {
    // ets_printf("D: 0x%08x %s = %s\n", e->client, e->dns.name, ipaddr_ntoa(&e->dns.addr));
    e->client->_dns_found(&e->dns.addr);
  }
  _free_event(e);
}

static void _reset_tcp_callbacks(tcp_pcb *pcb, AsyncClient *client) {
  tcp_arg(pcb, NULL);
  tcp_sent(pcb, NULL);
  tcp_recv(pcb, NULL);
  tcp_err(pcb, NULL);
  tcp_poll(pcb, NULL, 0);
  if (client) {
    _remove_events_for_client(client);
  }
}

static int8_t _tcp_connected(void *arg, tcp_pcb *pcb, int8_t err) {
  // ets_printf("+C: 0x%08x\n", pcb);
  AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);
  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_CONNECTED, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return ERR_MEM;
  }
  e->connected.pcb = pcb;
  e->connected.err = err;
  queue_mutex_guard guard;
  _send_async_event(e);
  return ERR_OK;
}

int8_t AsyncTCP_detail::tcp_poll(void *arg, struct tcp_pcb *pcb) {
  // throttle polling events queueing when event queue is getting filled up, let it handle _onack's
  {
    queue_mutex_guard guard;
    // async_tcp_log_d("qs:%u", _async_queue.size());
    if (_async_queue.size() > (_xor_shift_next() % CONFIG_ASYNC_TCP_QUEUE_SIZE / 2 + CONFIG_ASYNC_TCP_QUEUE_SIZE / 4)) {
      async_tcp_log_d("throttling");
      return ERR_OK;
    }
  }

  // ets_printf("+P: 0x%08x\n", pcb);
  AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);
  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_POLL, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return ERR_MEM;
  }
  e->poll.pcb = pcb;

  queue_mutex_guard guard;
  _send_async_event(e);
  return ERR_OK;
}

int8_t AsyncTCP_detail::tcp_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, int8_t err) {
  AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);
  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_RECV, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return ERR_MEM;
  }
  if (pb) {
    // ets_printf("+R: 0x%08x\n", pcb);
    e->recv.pcb = pcb;
    e->recv.pb = pb;
    e->recv.err = err;
  } else {
    // ets_printf("+F: 0x%08x\n", pcb);
    e->event = LWIP_TCP_FIN;
    e->fin.pcb = pcb;
    e->fin.err = err;
  }

  queue_mutex_guard guard;
  _send_async_event(e);
  return ERR_OK;
}

int8_t AsyncTCP_detail::tcp_sent(void *arg, struct tcp_pcb *pcb, uint16_t len) {
  // ets_printf("+S: 0x%08x\n", pcb);
  AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);
  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_SENT, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return ERR_MEM;
  }
  e->sent.pcb = pcb;
  e->sent.len = len;

  queue_mutex_guard guard;
  _send_async_event(e);
  return ERR_OK;
}

void AsyncTCP_detail::tcp_error(void *arg, int8_t err) {
  // ets_printf("+E: 0x%08x\n", arg);
  AsyncClient *client = reinterpret_cast<AsyncClient *>(arg);
  if (client && client->_pcb) {
    // The pcb has already been freed by LwIP; do not attempt to clear the callbacks!
    _remove_events_for_client(client);
    client->_pcb = nullptr;
  }

  // enqueue event to be processed in the async task for the user callback
  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_ERROR, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return;
  }
  e->error.err = err;

  queue_mutex_guard guard;
  _send_async_event(e);
}

static void _tcp_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
  // ets_printf("+DNS: name=%s ipaddr=0x%08x arg=%x\n", name, ipaddr, arg);
  auto client = reinterpret_cast<AsyncClient *>(arg);

  lwip_tcp_event_packet_t *e = new (std::nothrow) lwip_tcp_event_packet_t{LWIP_TCP_DNS, client};
  if (!e) {
    async_tcp_log_e("Failed to allocate event packet");
    return;
  }

  e->dns.name = name;
  if (ipaddr) {
    memcpy(&e->dns.addr, ipaddr, sizeof(ip_addr_t));
  } else {
    memset(&e->dns.addr, 0, sizeof(e->dns.addr));
  }

  queue_mutex_guard guard;
  _send_async_event(e);
}

static err_t _tcp_close_api(struct tcpip_api_call_data *api_call_msg) {
  // Unlike the other calls, this is not a direct wrapper of the LwIP function;
  // we perform the AsyncClient teardown interlocked safely with the LwIP task.

  // As a postcondition, the queue must not have any events referencing
  // the AsyncClient in api_call_msg->close.  This is because it is possible for
  // an error event to have been queued, clearing the pcb*, but after the async
  // thread has committed to closing/destructing the AsyncClient object.

  tcp_api_call_t *msg = (tcp_api_call_t *)api_call_msg;
  msg->err = ERR_CONN;
  if (*msg->pcb) {
    tcp_pcb *pcb = *msg->pcb;
    _reset_tcp_callbacks(pcb, msg->close);
    if (tcp_close(pcb) != ERR_OK) {
      // We do not permit failure here: abandon the pcb anyways.
      tcp_abort(pcb);
    }
    msg->err = ERR_OK;
    *msg->pcb = nullptr;  // PCB is now the property of LwIP
  } else {
    // Ensure there is not an error event queued for this client
    if (_remove_events_for_client(msg->close)) {
      msg->err = ERR_OK;  // dispose needs to be run
    }
  }
  return msg->err;
}

static esp_err_t _tcp_close(tcp_pcb **pcb, AsyncClient *client) {
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.close = client;
  tcpip_api_call(_tcp_close_api, (struct tcpip_api_call_data *)&msg);
  return msg.err;
}

void AsyncClient::close() {
  if (_pcb) {
    _tcp_recved(&_pcb, _rx_ack_len);
  }
  _close();
}

int8_t AsyncClient::_close() {
  // ets_printf("X: 0x%08x\n", (uint32_t)this);
  int8_t err = _tcp_close(&_pcb, this);
  // _pcb is now NULL
  if ((err == ERR_OK) && _discard_cb) {
    // _pcb was closed here
    async_tcp_log_elapsed("onDisconnect", _discard_cb(_discard_cb_arg, this));
  }
  return err;
}

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
