#include "SDK/RemoteWorkstation.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<RemoteWorkstation::Host> host;std::unique_ptr<RemoteWorkstation::Viewer> viewer;
RemoteWorkstation::Host& Host(){if(!host)host=std::make_unique<RemoteWorkstation::Host>();return *host;}
RemoteWorkstation::Viewer& Viewer(){if(!viewer)viewer=std::make_unique<RemoteWorkstation::Viewer>();return *viewer;}
RemoteWorkstation::Frame Frame(const Json& a){RemoteWorkstation::Frame f;f.width=Number(a,"width",0,3840);f.height=Number(a,"height",0,2160);f.sequence=a.contains("sequence")?Identifier(a,"sequence"):0;f.bgra=Bytes(a.at("bgra"),16*1024*1024);return f;}
Json FrameJson(const RemoteWorkstation::Frame& f){return {{"width",f.width},{"height",f.height},{"sequence",std::to_string(f.sequence)},{"bgra",f.bgra}};}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto frame=Frame(a.at("frame"));return {{"valid",RemoteWorkstation::ValidFrame(frame)}};}
if(op=="encode_frame"){auto frame=Frame(a.at("frame"));Require(RemoteWorkstation::ValidFrame(frame),"Invalid BGRA frame");return {{"data",RemoteWorkstation::EncodeFrame(frame)}};}
if(op=="decode_frame")return FrameJson(RemoteWorkstation::DecodeFrame(Bytes(a.at("data"),16*1024*1024)));
if(op=="verification"){CheckMessage(RemoteWorkstation::Verification(a.at("key").get<std::string>()));return Json::object();}
if(op=="host_start"){RemoteWorkstation::HostOptions options;options.address=a.value("address",std::string("127.0.0.1"));options.port=static_cast<std::uint16_t>(Number(a,"port",48380,65535));options.width=Number(a,"width",960,3840);options.height=Number(a,"height",540,2160);options.framesPerSecond=Number(a,"fps",15,60);options.allowControl=a.value("allow_control",false);auto width=options.width,height=options.height;options.capture=[width,height]{return RemoteWorkstation::CaptureDesktop(width,height);};if(options.allowControl)options.input=[](const RemoteWorkstation::Input& input){RemoteWorkstation::DeliverInput(input);};CheckMessage(Host().Start(std::move(options)));return {{"port",Host().Port()}};}
if(op=="host_stop"){if(host)host->Stop();return Json::object();}
if(op=="host_status")return {{"status",Host().Status()}};
if(op=="pairing_code")return {{"code",Host().PairingCode()}};
if(op=="viewer_connect"){CheckMessage(Viewer().Connect(a.at("address").get<std::string>(),static_cast<std::uint16_t>(Number(a,"port",48380,65535)),a.at("pairing_code").get<std::string>()));return {{"control_allowed",Viewer().ControlAllowed()}};}
if(op=="viewer_stop"){if(viewer)viewer->Stop();return Json::object();}
if(op=="receive"){std::vector<RemoteWorkstation::Input> inputs;auto array=a.value("inputs",Json::array());Require(array.is_array() && array.size()<=256,"Too many input events");for(const auto& value:array){RemoteWorkstation::Input i;i.kind=static_cast<RemoteWorkstation::InputKind>(Number(value,"kind",1,4));i.x=value.contains("x")?FromWire<std::int32_t>(value.at("x")):0;i.y=value.contains("y")?FromWire<std::int32_t>(value.at("y")):0;i.code=Number(value,"code");i.down=Number(value,"down",0,1);Require(RemoteWorkstation::ValidInput(i),"Invalid remote input");inputs.push_back(i);}return FrameJson(Viewer().Receive(inputs));}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned remote_workstation_sdk_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned remote_workstation_sdk_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* remote_workstation_sdk_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void remote_workstation_sdk_json_free(char* response)noexcept{delete[] response;}
