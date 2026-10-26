#include "../SDK/Transport.h"
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <exception>
int main(){
 try{
 TitanTransport::Runtime runtime;TitanTransport::Identity identity;
 const auto pin=identity.Fingerprint();if(!TitanTransport::IsFingerprint(pin))throw std::runtime_error("Invalid generated fingerprint");
 unsigned checks=1;const auto secret=TitanTransport::RandomSecret();if(!TitanTransport::IsSecret(secret)||TitanTransport::ConstantTimeEqual(secret,std::string(32,'g')))throw std::runtime_error("Secret check failed");++checks;
 for(bool invalidPin:{false,true}){
  uint16_t port=0;SOCKET listener=TitanTransport::Listen("127.0.0.1",0,&port);std::exception_ptr error;std::atomic<bool> closed{false};
  std::thread server([&]{try{SOCKET socket=accept(listener,nullptr,nullptr);if(socket==INVALID_SOCKET)throw std::runtime_error("Accept failed");TitanTransport::Channel peer(socket,identity);std::array<unsigned char,70001> data{};peer.Receive(data.data(),data.size());for(size_t i=0;i<data.size();++i)if(data[i]!=static_cast<unsigned char>(i*31u))throw std::runtime_error("TLS request bytes changed");peer.Send(data.data(),data.size());peer.Close();closed=true;}catch(...){error=std::current_exception();}});
  bool refused=false;
  try{auto actual=pin;if(invalidPin)actual[0]=actual[0]=='0'?'1':'0';TitanTransport::Channel peer(TitanTransport::Connect("127.0.0.1",port),actual);std::array<unsigned char,70001> data{};for(size_t i=0;i<data.size();++i)data[i]=static_cast<unsigned char>(i*31u);peer.Send(data.data(),data.size());std::array<unsigned char,70001> response{};peer.Receive(response.data(),response.size());if(response!=data)throw std::runtime_error("TLS roundtrip differs");++checks;}catch(const std::exception&){refused=true;}
  server.join();closesocket(listener);
  if(invalidPin){if(!refused)throw std::runtime_error("Mismatched pin was accepted");++checks;}else{if(refused){if(error)std::rethrow_exception(error);throw std::runtime_error("Valid TLS connection refused");}if(error)std::rethrow_exception(error);if(!closed)throw std::runtime_error("TLS owner cleanup failed");++checks;}
 }
 {
  // accept inherits the listener's nonblocking mode on Windows. A channel must
  // restore its documented synchronous mode, while the listener stays pollable.
  // Repeated healthy operations must outlive a single five-second I/O budget.
  uint16_t port=0;SOCKET listener=TitanTransport::Listen("127.0.0.1",0,&port);u_long nonblocking=1;
  if(ioctlsocket(listener,FIONBIO,&nonblocking))throw std::runtime_error("Nonblocking listener fixture failed");
  std::exception_ptr error;std::atomic<unsigned> completed{0};const auto started=GetTickCount64();
  std::thread server([&]{try{SOCKET socket=INVALID_SOCKET;auto until=GetTickCount64()+5000;while(socket==INVALID_SOCKET&&GetTickCount64()<until){socket=accept(listener,nullptr,nullptr);if(socket==INVALID_SOCKET){if(WSAGetLastError()!=WSAEWOULDBLOCK)throw std::runtime_error("Nonblocking accept failed");Sleep(1);}}if(socket==INVALID_SOCKET)throw std::runtime_error("Nonblocking accept timed out");TitanTransport::Channel peer(socket,identity);for(unsigned i=0;i<256;++i){uint32_t value=0;peer.Receive(&value,sizeof(value));if(value!=i)throw std::runtime_error("Long-session sequence changed");Sleep(25);peer.Send(&value,sizeof(value));++completed;}}catch(...){error=std::current_exception();}});
  std::exception_ptr clientError;try{TitanTransport::Channel peer(TitanTransport::Connect("127.0.0.1",port),pin);for(uint32_t i=0;i<256;++i){peer.Send(&i,sizeof(i));uint32_t value=0;peer.Receive(&value,sizeof(value));if(value!=i)throw std::runtime_error("Long-session echo changed");}}catch(...){clientError=std::current_exception();}
  server.join();closesocket(listener);if(error)std::rethrow_exception(error);if(clientError)std::rethrow_exception(clientError);if(completed!=256||GetTickCount64()-started<6000)throw std::runtime_error("Healthy TLS session did not span multiple I/O budgets");++checks;
 }
 {
  // A peer drip-feeds an incomplete, otherwise plausible TLS record. Activity
  // must not reset the absolute handshake deadline.
  uint16_t port=0;SOCKET listener=TitanTransport::Listen("127.0.0.1",0,&port);std::atomic<ULONGLONG> elapsed{0};std::atomic<bool> rejected{false};
  std::thread server([&]{SOCKET socket=accept(listener,nullptr,nullptr);auto start=GetTickCount64();try{TitanTransport::Channel peer(socket,identity);}catch(const std::exception&){rejected=true;}elapsed=GetTickCount64()-start;});
  SOCKET peer=TitanTransport::Connect("127.0.0.1",port);const unsigned char bytes[]{0x16,0x03,0x03,0x00,0x40,0x01,0x00,0x00,0x3c,0x03,0x03,0,0,0,0,0,0};
  for(auto byte:bytes){if(send(peer,reinterpret_cast<const char*>(&byte),1,0)!=1)break;Sleep(400);}closesocket(peer);server.join();closesocket(listener);
  if(!rejected||elapsed<4000||elapsed>7500)throw std::runtime_error("Trickled TLS handshake escaped its absolute deadline");++checks;
 }
 {
  uint16_t port=0;SOCKET listener=TitanTransport::Listen("127.0.0.1",0,&port);std::atomic<ULONGLONG> elapsed{0};std::atomic<bool> established{false},rejected{false};
  std::thread server([&]{try{TitanTransport::Channel channel(accept(listener,nullptr,nullptr),identity);established=true;auto start=GetTickCount64();std::array<unsigned char,16> data{};try{channel.Receive(data.data(),data.size(),1000);}catch(const std::exception&){rejected=true;}elapsed=GetTickCount64()-start;}catch(...){}});
  try{TitanTransport::Channel channel(TitanTransport::Connect("127.0.0.1",port),pin);unsigned char byte=7;for(unsigned i=0;i<16;++i){channel.Send(&byte,1);Sleep(200);}}catch(const std::exception&){}
  server.join();closesocket(listener);if(!established||!rejected||elapsed<700||elapsed>2500)throw std::runtime_error("Trickled application bytes escaped the complete-message deadline");++checks;
 }
 for(auto v:{"","127.0.0.999","localhost","1.2.3.4\n"}){bool refused=false;try{auto s=TitanTransport::Listen(v,0);closesocket(s);}catch(const std::exception&){refused=true;}if(!refused)throw std::runtime_error("Invalid IPv4 accepted");++checks;}
 std::cout<<checks<<" real pinned-TLS loopback/refusal checks passed; temporary per-session current-user keys, deleted on normal cleanup, no screen/input access, no firewall/trust changes.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
