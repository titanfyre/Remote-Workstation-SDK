#pragma once
#include "Transport.h"
#include "License.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RemoteWorkstation {
constexpr unsigned Product=8;
struct Frame {uint32_t width=0,height=0;uint64_t sequence=0;std::vector<uint8_t> bgra;};
enum class InputKind:uint32_t {Pointer=1,Button=2,Key=3,Wheel=4};
// Pointer x/y: normalized 0..65535. Button: code1 left/2 right/3 middle,
// down0/1. Key: Windows VK1..255, down0/1. Wheel: signed delta -1200..1200.
struct Input {InputKind kind=InputKind::Pointer;int32_t x=0,y=0;uint32_t code=0,down=0;};
struct HostOptions {
 std::string address="127.0.0.1";uint16_t port=48380;
 unsigned width=960,height=540,framesPerSecond=15;
 bool allowControl=false;
 // Optional bounded provider/sink for an application's own pixels/controls.
 // Callbacks must not re-enter Host lifecycle methods or destroy that Host.
 // The default captures the primary Windows desktop and, only with explicit
 // allowControl, delivers standard same-session Windows SendInput events.
 std::function<Frame()> capture;
 std::function<void(const Input&)> input;
};
class Host {
 struct State;std::unique_ptr<State> state_;
public:
 Host();~Host();Host(const Host&)=delete;Host& operator=(const Host&)=delete;
 std::string Start(HostOptions options={});
 void Stop() noexcept;
 std::string PairingCode(); // explicit user sharing only; contains session secret.
 uint16_t Port();
 std::string Status();
};
class Viewer {
 struct State;std::unique_ptr<State> state_;
public:
 Viewer();~Viewer();Viewer(const Viewer&)=delete;
 std::string Connect(std::string_view numericIPv4,uint16_t port,std::string_view pairingCode);
 void Stop() noexcept;
 // One in-flight request at a time. Each call obtains a new frame and sends
 // at most64 validated input events; no accumulating capture/send queue.
 Frame Receive(const std::vector<Input>& inputs={});
 bool ControlAllowed();
};
std::string Verification(std::string_view key); // empty on success; detailed English error.
Frame CaptureDesktop(unsigned maxWidth=960,unsigned maxHeight=540);
void DeliverInput(const Input& input);
bool ValidInput(const Input& input) noexcept;
bool ValidFrame(const Frame& frame) noexcept;
// Public deterministic codecs for protocol tests and fixture integration.
std::vector<uint8_t> EncodeFrame(const Frame& frame);
Frame DecodeFrame(const std::vector<uint8_t>& encoded);
}
