#include "RemoteWorkstation.h"
#include <compressapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <set>

namespace RemoteWorkstation {
namespace {
constexpr uint32_t Magic=0x31575352,MaxBody=4*1024*1024;
enum:uint32_t {Hello=1,Approved=2,Request=3,Image=4,Refused=5};
struct Packet{uint32_t magic=Magic,version=1,type=0,bytes=0;};
struct ImageHeader{uint32_t width,height,bytes,reserved;uint64_t sequence;};
static_assert(sizeof(Packet)==16&&sizeof(Input)==20&&sizeof(ImageHeader)==24);
std::string Failure(const char* stage){return std::string(stage)+" failed (Windows error "+std::to_string(GetLastError())+").";}
void Check(bool ok,const char* stage){if(!ok)throw std::runtime_error(Failure(stage));}
std::shared_ptr<TitanTransport::Channel> OwnChannel(SOCKET socket){
 // A failed shared-control-block allocation must not strand an accepted socket.
 auto raw=new(std::nothrow) TitanTransport::Channel(socket);
 if(!raw){closesocket(socket);throw std::bad_alloc();}
 return std::shared_ptr<TitanTransport::Channel>(raw);
}
void Send(TitanTransport::Channel& channel,uint32_t type,const void* data,size_t size){if(size>MaxBody)throw std::runtime_error("Remote packet exceeds its bound.");Packet header{};header.type=type;header.bytes=static_cast<uint32_t>(size);channel.Send(&header,sizeof(header));if(size)channel.Send(data,size);}
std::vector<uint8_t> ReceivePacket(TitanTransport::Channel& channel,uint32_t type,size_t minimum,size_t maximum){const auto started=GetTickCount64();Packet header{};channel.Receive(&header,sizeof(header));if(header.magic==Magic&&header.version==1&&header.type==Refused&&!header.bytes)throw std::runtime_error("The host refused the viewer's session pairing secret.");if(header.magic!=Magic||header.version!=1||header.type!=type||header.bytes<minimum||header.bytes>maximum||header.bytes>MaxBody)throw std::runtime_error("Unexpected or oversized remote protocol packet.");std::vector<uint8_t> bytes(header.bytes);if(header.bytes){auto elapsed=GetTickCount64()-started;if(elapsed>=5000)throw std::runtime_error("Remote packet receive deadline exceeded.");channel.Receive(bytes.data(),bytes.size(),static_cast<uint32_t>(5000-elapsed));}return bytes;}
void Access(bool begin){TS_STATUS status{};auto error=begin?TitanSdk::Begin(Product,status):TitanSdk::Status(Product,status);if(!error.empty())throw std::runtime_error(error);if(status.mode!=TITAN_SDK_MODE_DEMO&&status.mode!=TITAN_SDK_MODE_FULL)throw std::runtime_error("Remote Workstation demo has expired. Run Verification(key) with an enabled product key.");}
struct Compression{COMPRESSOR_HANDLE handle=nullptr;bool compressor;explicit Compression(bool encode):compressor(encode){Check(encode?CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)!=FALSE:CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)!=FALSE,"Frame codec initialization");}~Compression(){if(handle){if(compressor)CloseCompressor(handle);else CloseDecompressor(handle);}}};
class Pressed {
 std::function<void(const Input&)> sink_;std::set<uint32_t> keys_,buttons_;
public:
 explicit Pressed(std::function<void(const Input&)> sink):sink_(std::move(sink)){}
 ~Pressed(){for(auto key:keys_)try{sink_(Input{InputKind::Key,0,0,key,0});}catch(...){}for(auto button:buttons_)try{sink_(Input{InputKind::Button,0,0,button,0});}catch(...){}}
 void Put(const Input& input){
  if(input.kind==InputKind::Key){if(input.down)keys_.insert(input.code);else keys_.erase(input.code);}
  if(input.kind==InputKind::Button){if(input.down)buttons_.insert(input.code);else buttons_.erase(input.code);}
  sink_(input);
 }
};
}
bool ValidInput(const Input& i)noexcept{
 switch(i.kind){
 case InputKind::Pointer:return i.x>=0&&i.x<=65535&&i.y>=0&&i.y<=65535&&!i.code&&!i.down;
 case InputKind::Button:return !i.x&&!i.y&&i.code>=1&&i.code<=3&&i.down<=1;
 case InputKind::Key:return !i.x&&!i.y&&i.code>=1&&i.code<=255&&i.down<=1;
 case InputKind::Wheel:return !i.x&&i.y>=-1200&&i.y<=1200&&!i.code&&!i.down;
 default:return false;
 }
}
bool ValidFrame(const Frame& f)noexcept{return f.width>=16&&f.width<=1280&&f.height>=16&&f.height<=720&&f.bgra.size()==static_cast<size_t>(f.width)*f.height*4;}
std::vector<uint8_t> EncodeFrame(const Frame& frame){if(!ValidFrame(frame))throw std::runtime_error("Frame dimensions or BGRA byte count are invalid.");Compression encoder(true);SIZE_T required=0;if(Compress(encoder.handle,frame.bgra.data(),frame.bgra.size(),nullptr,0,&required)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||required>MaxBody-sizeof(ImageHeader))throw std::runtime_error("Frame compression size is invalid.");std::vector<uint8_t> data(sizeof(ImageHeader)+required);SIZE_T written=0;Check(Compress(encoder.handle,frame.bgra.data(),frame.bgra.size(),data.data()+sizeof(ImageHeader),required,&written)!=FALSE,"Frame compression");if(written>required)throw std::runtime_error("Frame codec exceeded its bound.");data.resize(sizeof(ImageHeader)+written);ImageHeader h{frame.width,frame.height,static_cast<uint32_t>(frame.bgra.size()),0,frame.sequence};std::memcpy(data.data(),&h,sizeof(h));return data;}
Frame DecodeFrame(const std::vector<uint8_t>& bytes){if(bytes.size()<sizeof(ImageHeader)||bytes.size()>MaxBody)throw std::runtime_error("Invalid compressed frame length.");ImageHeader h{};std::memcpy(&h,bytes.data(),sizeof(h));if(h.width<16||h.width>1280||h.height<16||h.height>720||h.reserved||h.bytes!=static_cast<uint64_t>(h.width)*h.height*4)throw std::runtime_error("Invalid compressed frame header.");Frame out;out.width=h.width;out.height=h.height;out.sequence=h.sequence;out.bgra.resize(h.bytes);Compression decoder(false);SIZE_T written=0;Check(Decompress(decoder.handle,bytes.data()+sizeof(h),bytes.size()-sizeof(h),out.bgra.data(),out.bgra.size(),&written)!=FALSE,"Frame decompression");if(written!=out.bgra.size())throw std::runtime_error("Frame decompression size differs.");return out;}
Frame CaptureDesktop(unsigned maxWidth,unsigned maxHeight){if(maxWidth<16||maxWidth>1280||maxHeight<16||maxHeight>720)throw std::runtime_error("Capture limit must be 16..1280 by 16..720.");const int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);if(width<=0||height<=0||width>32768||height>32768)throw std::runtime_error("Primary desktop dimensions are unavailable.");const double scale=(std::min)({1.0,static_cast<double>(maxWidth)/width,static_cast<double>(maxHeight)/height});Frame frame;frame.width=(std::max)(16u,static_cast<unsigned>(width*scale));frame.height=(std::max)(16u,static_cast<unsigned>(height*scale));frame.bgra.resize(static_cast<size_t>(frame.width)*frame.height*4);
 struct Screen{HDC dc=GetDC(nullptr);~Screen(){if(dc)ReleaseDC(nullptr,dc);}} screen;Check(screen.dc!=nullptr,"Primary desktop DC");
 struct Memory{HDC dc;HBITMAP image=nullptr;HGDIOBJ previous=nullptr;explicit Memory(HDC source):dc(CreateCompatibleDC(source)){}~Memory(){if(previous)SelectObject(dc,previous);if(image)DeleteObject(image);if(dc)DeleteDC(dc);}} memory(screen.dc);Check(memory.dc!=nullptr,"Capture DC");
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(frame.width);info.bmiHeader.biHeight=-static_cast<LONG>(frame.height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;memory.image=CreateDIBSection(screen.dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);Check(memory.image&&pixels,"Capture bitmap");memory.previous=SelectObject(memory.dc,memory.image);Check(memory.previous&&memory.previous!=HGDI_ERROR,"Capture bitmap selection");SetStretchBltMode(memory.dc,COLORONCOLOR);Check(StretchBlt(memory.dc,0,0,static_cast<int>(frame.width),static_cast<int>(frame.height),screen.dc,0,0,width,height,SRCCOPY|CAPTUREBLT)!=FALSE,"Primary desktop capture");GdiFlush();std::memcpy(frame.bgra.data(),pixels,frame.bgra.size());return frame;}
void DeliverInput(const Input& input){if(!ValidInput(input))throw std::runtime_error("Invalid remote input record.");INPUT out{};
 if(input.kind==InputKind::Key){out.type=INPUT_KEYBOARD;out.ki.wVk=static_cast<WORD>(input.code);out.ki.dwFlags=input.down?0:KEYEVENTF_KEYUP;}
 else{out.type=INPUT_MOUSE;switch(input.kind){case InputKind::Pointer:out.mi.dx=input.x;out.mi.dy=input.y;out.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE;break;case InputKind::Button:if(input.code==1)out.mi.dwFlags=input.down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;else if(input.code==2)out.mi.dwFlags=input.down?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP;else out.mi.dwFlags=input.down?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;break;case InputKind::Wheel:out.mi.mouseData=static_cast<DWORD>(input.y);out.mi.dwFlags=MOUSEEVENTF_WHEEL;break;default:break;}}
 if(SendInput(1,&out,sizeof(out))!=1)throw std::runtime_error("Windows refused remote input. UIPI, secure desktop and session boundaries remain enforced.");}
std::string Verification(std::string_view key){return TitanSdk::Verification(Product,key);}

struct Host::State{
 TitanTransport::Runtime network;std::unique_ptr<TitanTransport::Identity> identity;std::thread worker;std::atomic<bool> stop{true};std::mutex mutex,lifecycle;SOCKET listener=INVALID_SOCKET;std::shared_ptr<TitanTransport::Channel> active;std::string secret,status="Stopped";uint16_t port=0;HostOptions options;
 void Note(std::string value){std::lock_guard<std::mutex> lock(mutex);status=std::move(value);}
 void Run()noexcept{
  try{
   while(!stop){fd_set sockets;FD_ZERO(&sockets);FD_SET(listener,&sockets);timeval wait{0,200000};int selected=select(0,&sockets,nullptr,nullptr,&wait);if(selected==0)continue;if(selected<0){if(stop)break;throw std::runtime_error("Host listener failed.");}SOCKET socket=accept(listener,nullptr,nullptr);if(socket==INVALID_SOCKET){if(stop)break;continue;}
    try{
     auto peer=OwnChannel(socket);{std::lock_guard<std::mutex> lock(mutex);active=peer;}if(stop){peer->Close();break;}Note("Authenticating paired viewer...");peer->HandshakeServer(*identity);
     auto hello=ReceivePacket(*peer,Hello,32,32);std::string supplied(hello.begin(),hello.end());const bool accepted=TitanTransport::ConstantTimeEqual(supplied,secret);SecureZeroMemory(supplied.data(),supplied.size());SecureZeroMemory(hello.data(),hello.size());if(!accepted){Send(*peer,Refused,nullptr,0);throw std::runtime_error("Viewer pairing secret refused.");}
     Access(false);uint32_t allowed=options.allowControl?1u:0u;Send(*peer,Approved,&allowed,sizeof(allowed));Note(options.allowControl?"Connected - viewing and operator-enabled control":"Connected - view only");Pressed pressed(options.input?options.input:DeliverInput);uint64_t sequence=0,lastLicense=GetTickCount64();
     while(!stop){auto records=ReceivePacket(*peer,Request,0,64*sizeof(Input));if(records.size()%sizeof(Input))throw std::runtime_error("Invalid input packet alignment.");std::vector<Input> inputs(records.size()/sizeof(Input));if(!records.empty())std::memcpy(inputs.data(),records.data(),records.size());for(const auto& input:inputs)if(!ValidInput(input))throw std::runtime_error("Invalid viewer input record.");if(!options.allowControl&&!inputs.empty())throw std::runtime_error("Remote input is disabled by the host operator.");if(GetTickCount64()-lastLicense>=500){Access(false);lastLicense=GetTickCount64();}for(const auto& input:inputs)pressed.Put(input);
      const auto begin=GetTickCount64();Frame frame=options.capture?options.capture():CaptureDesktop(options.width,options.height);frame.sequence=++sequence;auto encoded=EncodeFrame(frame);Send(*peer,Image,encoded.data(),encoded.size());auto elapsed=GetTickCount64()-begin;auto interval=1000u/options.framesPerSecond;if(elapsed<interval){auto delay=interval-static_cast<DWORD>(elapsed);for(DWORD waited=0;waited<delay&&!stop;waited+=10)Sleep((std::min)(DWORD(10),delay-waited));}
     }
    }catch(const std::exception& e){if(!stop)Note(e.what());}catch(...){if(!stop)Note("Remote session failed due to a local resource error.");}
    {std::lock_guard<std::mutex> lock(mutex);active.reset();}
   }
  }catch(const std::exception& e){Note(e.what());}catch(...){Note("Host stopped due to a local resource error.");}
  stop=true;
 }
};
Host::Host():state_(std::make_unique<State>()){}
Host::~Host(){Stop();}
std::string Host::Start(HostOptions options){try{Stop();std::lock_guard<std::mutex> lifecycle(state_->lifecycle);if(options.width<16||options.width>1280||options.height<16||options.height>720||options.framesPerSecond<1||options.framesPerSecond>30)throw std::runtime_error("Use dimensions 16..1280 by 16..720 and 1..30 frames/s.");Access(true);auto identity=std::make_unique<TitanTransport::Identity>();auto secret=TitanTransport::RandomSecret();uint16_t port=0;SOCKET listener=TitanTransport::Listen(options.address,options.port,&port);u_long nonblocking=1;if(ioctlsocket(listener,FIONBIO,&nonblocking)){auto code=WSAGetLastError();closesocket(listener);throw std::runtime_error("Host listener policy failed (Windows error "+std::to_string(code)+").");}state_->options=std::move(options);state_->identity=std::move(identity);state_->secret=std::move(secret);state_->listener=listener;state_->port=port;state_->stop=false;state_->Note("Listening - explicitly paired viewers only");try{state_->worker=std::thread([s=state_.get()]{s->Run();});}catch(...){state_->stop=true;closesocket(listener);state_->listener=INVALID_SOCKET;throw;}return {};}catch(const std::exception& e){Stop();return e.what();}catch(...){Stop();return "Host could not start due to a local resource error.";}}
void Host::Stop()noexcept{std::lock_guard<std::mutex> lifecycle(state_->lifecycle);state_->stop=true;{std::lock_guard<std::mutex> lock(state_->mutex);if(state_->active)state_->active->Close();}if(state_->worker.joinable())state_->worker.join();if(state_->listener!=INVALID_SOCKET){closesocket(state_->listener);state_->listener=INVALID_SOCKET;}state_->identity.reset();SecureZeroMemory(state_->secret.data(),state_->secret.size());state_->secret.clear();state_->port=0;state_->Note("Stopped");}
std::string Host::PairingCode(){std::lock_guard<std::mutex> lock(state_->lifecycle);return state_->identity?state_->identity->Fingerprint()+":"+state_->secret:std::string{};}
uint16_t Host::Port(){std::lock_guard<std::mutex> lock(state_->lifecycle);return state_->port;}
std::string Host::Status(){std::lock_guard<std::mutex> lock(state_->mutex);return state_->status;}

struct Viewer::State{TitanTransport::Runtime network;std::mutex mutex;std::shared_ptr<TitanTransport::Channel> peer;bool allowControl=false;uint64_t lastLicense=0,generation=0;std::mutex reader;};
Viewer::Viewer():state_(std::make_unique<State>()){}
Viewer::~Viewer(){Stop();}
std::string Viewer::Connect(std::string_view address,uint16_t port,std::string_view pairing){try{Stop();uint64_t generation=0;{std::lock_guard<std::mutex> lock(state_->mutex);generation=state_->generation;}if(pairing.size()!=97||pairing[64]!=':'||!TitanTransport::IsFingerprint(pairing.substr(0,64))||!TitanTransport::IsSecret(pairing.substr(65)))return "Paste the host's full 97-character pairing code (fingerprint:session-secret).";Access(true);auto peer=OwnChannel(TitanTransport::Connect(address,port));{std::lock_guard<std::mutex> lock(state_->mutex);if(generation!=state_->generation)throw std::runtime_error("Viewer connection was cancelled.");state_->peer=peer;}peer->HandshakeClient(pairing.substr(0,64));Send(*peer,Hello,pairing.data()+65,32);auto response=ReceivePacket(*peer,Approved,sizeof(uint32_t),sizeof(uint32_t));uint32_t control=0;std::memcpy(&control,response.data(),sizeof(control));if(control>1)throw std::runtime_error("Invalid host approval flags.");{std::lock_guard<std::mutex> lock(state_->mutex);if(generation!=state_->generation||state_->peer!=peer)throw std::runtime_error("Viewer connection was cancelled.");state_->allowControl=control!=0;state_->lastLicense=GetTickCount64();}return {};}catch(const std::exception& e){Stop();return e.what();}catch(...){Stop();return "Viewer connection failed due to a local resource error.";}}
void Viewer::Stop()noexcept{std::shared_ptr<TitanTransport::Channel> peer;{std::lock_guard<std::mutex> lock(state_->mutex);peer=std::move(state_->peer);state_->allowControl=false;++state_->generation;}if(peer)peer->Close();}
Frame Viewer::Receive(const std::vector<Input>& inputs){std::lock_guard<std::mutex> read(state_->reader);std::shared_ptr<TitanTransport::Channel> peer;bool allowed=false;{std::lock_guard<std::mutex> lock(state_->mutex);peer=state_->peer;allowed=state_->allowControl;}if(!peer)throw std::runtime_error("Viewer is not connected.");if(inputs.size()>64)throw std::runtime_error("Send at most 64 input records per request.");if(!allowed&&!inputs.empty())throw std::runtime_error("The host has not enabled remote control.");for(auto& input:inputs)if(!ValidInput(input))throw std::runtime_error("Invalid outgoing remote input.");if(GetTickCount64()-state_->lastLicense>=500){Access(false);state_->lastLicense=GetTickCount64();}Send(*peer,Request,inputs.data(),inputs.size()*sizeof(Input));auto data=ReceivePacket(*peer,Image,sizeof(ImageHeader)+1,MaxBody);return DecodeFrame(data);}
bool Viewer::ControlAllowed(){std::lock_guard<std::mutex> lock(state_->mutex);return state_->allowControl;}
}
