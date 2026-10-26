#include "../SDK/RemoteWorkstation.h"
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace {
constexpr COLORREF Black=RGB(13,20,32),Panel=RGB(24,35,52),Blue=RGB(39,105,242),White=RGB(239,245,255);
HWND Window{},Canvas{},Address{},Port{},Pairing{},Allow{},Start{},Stop{},Key{},Activate{},Status{},KeyLabel{},ModeHost{},ModeViewer{};
HFONT Font{},Title{},Mono{};HBRUSH PanelBrush{},BackgroundBrush{};
bool Preview=false,HostMode=true,Busy=false,Closing=false;std::atomic<bool> ViewerStop{true},FramePending{false};
std::wstring LastMessage;
std::string LastHostStatus;
std::thread Stream,Action;std::mutex Frames,Inputs;RemoteWorkstation::Frame Latest;std::deque<RemoteWorkstation::Input> Pending;
RemoteWorkstation::Host Host;RemoteWorkstation::Viewer Viewer;
#ifdef TITAN_OFFLINE_PREVIEW
const ULONGLONG PreviewStarted=GetTickCount64();
#endif
std::wstring Widen(std::string_view s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(n<=0)return L"The operation failed.";std::wstring value(static_cast<size_t>(n),L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),value.data(),n);return value;}
std::string Text(HWND h){int n=GetWindowTextLengthA(h);if(n<0||n>1024)return {};std::string s(static_cast<size_t>(n)+1,'\0');GetWindowTextA(h,s.data(),n+1);s.resize(static_cast<size_t>(n));return s;}
void Note(const std::wstring& s){if(LastMessage!=s){LastMessage=s;SetWindowTextW(Status,s.c_str());}}
void PostNote(std::string_view text)noexcept{try{auto value=std::make_unique<std::string>(text);if(PostMessageW(Window,WM_APP+2,0,reinterpret_cast<LPARAM>(value.get())))value.release();}catch(...){PostMessageW(Window,WM_APP+4,0,0);}}
HWND Control(const wchar_t* kind,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){HWND h=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,Window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(Font),TRUE);return h;}
void Buttons(){EnableWindow(Start,!Preview&&!Busy);EnableWindow(Activate,!Preview&&!Busy);EnableWindow(ModeHost,!Busy);EnableWindow(ModeViewer,!Busy);}
void CopyPair(){auto value=Text(Pairing);if(value.empty()||!OpenClipboard(Window))return;auto memory=GlobalAlloc(GMEM_MOVEABLE,(value.size()+1)*sizeof(wchar_t));if(memory){auto text=static_cast<wchar_t*>(GlobalLock(memory));if(text){for(size_t i=0;i<value.size();++i)text[i]=static_cast<wchar_t>(value[i]);text[value.size()]=0;GlobalUnlock(memory);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,memory))GlobalFree(memory);}else GlobalFree(memory);}CloseClipboard();}
void StopSessions(){ViewerStop=true;Viewer.Stop();if(Stream.joinable())Stream.join();Host.Stop();LastHostStatus.clear();EnableWindow(Allow,TRUE);{std::lock_guard<std::mutex> lock(Inputs);Pending.clear();}SetWindowTextW(Pairing,L"");Note(L"Stopped. No listener or remote-control session is active.");}
uint16_t PortValue(){auto text=Text(Port);if(text.empty()||text.size()>5)throw std::runtime_error("Enter a port from 1 to 65535.");unsigned value=0;for(char c:text){if(c<'0'||c>'9')throw std::runtime_error("Enter a numeric port.");value=value*10+static_cast<unsigned>(c-'0');}if(!value||value>65535)throw std::runtime_error("Enter a port from 1 to 65535.");return static_cast<uint16_t>(value);}
void Queue(RemoteWorkstation::Input input){if(HostMode||ViewerStop||SendMessageW(Allow,BM_GETCHECK,0,0)!=BST_CHECKED||!Viewer.ControlAllowed())return;if(!RemoteWorkstation::ValidInput(input))return;std::lock_guard<std::mutex> lock(Inputs);if(input.kind==RemoteWorkstation::InputKind::Pointer&&!Pending.empty()&&Pending.back().kind==input.kind)Pending.back()=input;else if(Pending.size()<64)Pending.push_back(input);else{ViewerStop=true;Viewer.Stop();Pending.clear();PostNote("Control queue reached its bound; disconnected so held inputs are released.");}}
RECT ImageRect(HWND h){RECT area{};GetClientRect(h,&area);std::lock_guard<std::mutex> lock(Frames);if(!Latest.width||!Latest.height)return area;double scale=(std::min)(double(area.right)/Latest.width,double(area.bottom)/Latest.height);LONG w=static_cast<LONG>(Latest.width*scale),height=static_cast<LONG>(Latest.height*scale);return {(area.right-w)/2,(area.bottom-height)/2,(area.right+w)/2,(area.bottom+height)/2};}
LRESULT CALLBACK CanvasProc(HWND h,UINT m,WPARAM w,LPARAM l){using namespace RemoteWorkstation;
 switch(m){case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT p{};HDC dc=BeginPaint(h,&p);RECT r{};GetClientRect(h,&r);FillRect(dc,&r,BackgroundBrush);RECT target=ImageRect(h);{std::lock_guard<std::mutex> lock(Frames);if(ValidFrame(Latest)){BITMAPINFO b{};b.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);b.bmiHeader.biWidth=static_cast<LONG>(Latest.width);b.bmiHeader.biHeight=-static_cast<LONG>(Latest.height);b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;b.bmiHeader.biCompression=BI_RGB;StretchDIBits(dc,target.left,target.top,target.right-target.left,target.bottom-target.top,0,0,static_cast<int>(Latest.width),static_cast<int>(Latest.height),Latest.bgra.data(),&b,DIB_RGB_COLORS,SRCCOPY);}else{SetBkMode(dc,TRANSPARENT);SetTextColor(dc,White);SelectObject(dc,Font);DrawTextW(dc,HostMode?L"Host starts only when you choose Start.\nShare the pairing code with an authorized viewer.":L"Connect with the host's address and pairing code.\nRemote control requires both operators' permission.",-1,&r,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}}EndPaint(h,&p);return 0;}
 case WM_MOUSEMOVE:{if(!HostMode){auto r=ImageRect(h);int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);if(r.right>r.left&&r.bottom>r.top&&x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)Queue(Input{InputKind::Pointer,static_cast<int32_t>((static_cast<int64_t>(x-r.left)*65535)/(r.right-r.left)),static_cast<int32_t>((static_cast<int64_t>(y-r.top)*65535)/(r.bottom-r.top)),0,0});}return 0;}
 case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:case WM_MBUTTONDOWN:SetFocus(h);SetCapture(h);Queue(Input{InputKind::Button,0,0,m==WM_LBUTTONDOWN?1u:m==WM_RBUTTONDOWN?2u:3u,1});return 0;
 case WM_LBUTTONUP:case WM_RBUTTONUP:case WM_MBUTTONUP:ReleaseCapture();Queue(Input{InputKind::Button,0,0,m==WM_LBUTTONUP?1u:m==WM_RBUTTONUP?2u:3u,0});return 0;
 case WM_MOUSEWHEEL:Queue(Input{InputKind::Wheel,0,GET_WHEEL_DELTA_WPARAM(w),0,0});return 0;
 case WM_KEYDOWN:case WM_SYSKEYDOWN:if((l&(1ll<<30))==0)Queue(Input{InputKind::Key,0,0,static_cast<uint32_t>(w),1});return 0;
 case WM_KEYUP:case WM_SYSKEYUP:Queue(Input{InputKind::Key,0,0,static_cast<uint32_t>(w),0});return 0;
 case WM_KILLFOCUS:if(!HostMode&&!ViewerStop){for(uint32_t key=1;key<=255;++key)if(GetAsyncKeyState(static_cast<int>(key))&0x8000)Queue(Input{InputKind::Key,0,0,key,0});for(uint32_t button=1;button<=3;++button)Queue(Input{InputKind::Button,0,0,button,0});}return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
void Mode(bool host){StopSessions();HostMode=host;SetWindowTextW(Start,host?L"Start host":L"Connect viewer");SetWindowTextW(Allow,host?L"Allow keyboard and mouse control":L"Send keyboard and mouse (click view)");SendMessageW(Allow,BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(Pairing,EM_SETREADONLY,host,0);SetWindowTextW(Address,L"127.0.0.1");{std::lock_guard<std::mutex> lock(Frames);Latest={};}InvalidateRect(Canvas,nullptr,FALSE);InvalidateRect(ModeHost,nullptr,TRUE);InvalidateRect(ModeViewer,nullptr,TRUE);}
void StartSession(){if(Busy||Preview)return;try{auto address=Text(Address);auto port=PortValue();const bool allowed=SendMessageW(Allow,BM_GETCHECK,0,0)==BST_CHECKED;if(HostMode){RemoteWorkstation::HostOptions options;options.address=address;options.port=port;options.allowControl=allowed;auto e=Host.Start(std::move(options));if(!e.empty())Note(Widen(e));else{SetWindowTextA(Pairing,Host.PairingCode().c_str());Note(L"Host is visible and listening. Share its pairing code privately. No firewall rule was changed.");}}else{auto pair=Text(Pairing);ViewerStop=true;Viewer.Stop();if(Stream.joinable())Stream.join();ViewerStop=false;Stream=std::thread([address=std::move(address),port,pair=std::move(pair)]() mutable{try{auto error=Viewer.Connect(address,port,pair);SecureZeroMemory(pair.data(),pair.size());if(!error.empty()){ViewerStop=true;PostNote(error);return;}PostNote(Viewer.ControlAllowed()?"Connected. Enable Send keyboard and mouse only if you intend to control this host.":"Connected in view-only mode; the host has disabled control.");try{while(!ViewerStop){std::vector<RemoteWorkstation::Input> inputs;{std::lock_guard<std::mutex> lock(Inputs);while(!Pending.empty()&&inputs.size()<64){inputs.push_back(Pending.front());Pending.pop_front();}}auto frame=Viewer.Receive(inputs);{std::lock_guard<std::mutex> lock(Frames);Latest=std::move(frame);}if(!FramePending.exchange(true))PostMessageW(Window,WM_APP+1,0,0);}}catch(const std::exception& e){if(!ViewerStop)PostNote(e.what());}Viewer.Stop();ViewerStop=true;}catch(...){SecureZeroMemory(pair.data(),pair.size());Viewer.Stop();ViewerStop=true;PostNote("Viewer stopped due to a local resource error.");}});Note(L"Connecting with pinned TLS and the host's session secret...");}}catch(const std::exception& e){Note(Widen(e.what()));}}
void Verification(){
 if(Busy||Preview)return;
 if(Action.joinable())Action.join();
 auto key=Text(Key);SetWindowTextW(Key,L"");Busy=true;Buttons();Note(L"Verifying the product key over HTTPS...");
 try{Action=std::thread([key=std::move(key)]() mutable{
  try{auto error=RemoteWorkstation::Verification(key);SecureZeroMemory(key.data(),key.size());auto value=std::make_unique<std::string>(std::move(error));if(PostMessageW(Window,WM_APP+3,0,reinterpret_cast<LPARAM>(value.get())))value.release();}
  catch(...){SecureZeroMemory(key.data(),key.size());PostMessageW(Window,WM_APP+4,0,0);}
 });}catch(...){SecureZeroMemory(key.data(),key.size());Busy=false;Buttons();Note(L"Could not start the verification worker.");}
}
LRESULT CALLBACK ProcImpl(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:Window=h;Font=CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");Title=CreateFontW(30,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");Mono=CreateFontW(15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
  {auto t=Control(L"STATIC",L"Remote Workstation",0,24,20,760,40,0);SendMessageW(t,WM_SETFONT,reinterpret_cast<WPARAM>(Title),TRUE);}
  Control(L"STATIC",L"A native C++ SDK for explicit, paired desktop sessions",0,26,65,900,24,0);
  ModeHost=Control(L"BUTTON",L"Host",BS_OWNERDRAW|WS_TABSTOP,24,108,145,40,1);ModeViewer=Control(L"BUTTON",L"Viewer",BS_OWNERDRAW|WS_TABSTOP,177,108,145,40,2);
  Control(L"STATIC",L"Numeric IPv4 / bind address",0,24,174,305,24,0);Address=Control(L"EDIT",L"127.0.0.1",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,24,202,305,34,10);SendMessageW(Address,EM_SETLIMITTEXT,15,0);
  Control(L"STATIC",L"Port",0,24,249,90,24,0);Port=Control(L"EDIT",L"48380",WS_BORDER|ES_AUTOHSCROLL|ES_NUMBER|WS_TABSTOP,120,244,209,34,11);SendMessageW(Port,EM_SETLIMITTEXT,5,0);
  Allow=Control(L"BUTTON",L"Allow keyboard and mouse control",BS_AUTOCHECKBOX|WS_TABSTOP,24,295,310,42,12);
  Start=Control(L"BUTTON",L"Start host",BS_OWNERDRAW|WS_TABSTOP,24,350,145,40,3);Stop=Control(L"BUTTON",L"Stop",BS_OWNERDRAW|WS_TABSTOP,177,350,145,40,4);
  Control(L"STATIC",L"Pairing code  /  share privately",0,24,414,305,24,0);Pairing=Control(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_TABSTOP,24,444,305,100,13);SendMessageW(Pairing,EM_SETLIMITTEXT,97,0);SendMessageW(Pairing,WM_SETFONT,reinterpret_cast<WPARAM>(Mono),TRUE);Control(L"BUTTON",L"Copy pairing code",BS_OWNERDRAW|WS_TABSTOP,24,556,305,36,5);
  Canvas=CreateWindowExW(0,L"TitanRemoteCanvas",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,352,108,650,482,h,nullptr,nullptr,nullptr);
  KeyLabel=Control(L"STATIC",L"Product key  /  unlock full use until Windows restarts",0,24,618,900,24,0);Key=Control(L"EDIT",L"",WS_BORDER|ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP,24,650,740,34,14);SendMessageW(Key,EM_SETLIMITTEXT,128,0);Activate=Control(L"BUTTON",L"Verification",BS_OWNERDRAW|WS_TABSTOP,780,650,222,34,6);
  Status=Control(L"STATIC",Preview?L"Offline preview - no capture, input injection, listener or driver access.":L"Stopped. The runtime's one-hour demo starts on the first real Host or Viewer session.",0,24,704,980,60,15);
  if(Preview){RemoteWorkstation::Frame frame;frame.width=640;frame.height=360;frame.bgra.resize(640u*360*4);for(unsigned y=0;y<360;++y)for(unsigned x=0;x<640;++x){auto p=(y*640+x)*4;frame.bgra[p]=static_cast<uint8_t>(32+x/5);frame.bgra[p+1]=static_cast<uint8_t>(36+y/4);frame.bgra[p+2]=static_cast<uint8_t>(18+x/8);frame.bgra[p+3]=255;}Latest=std::move(frame);}
  Buttons();SetTimer(h,1,500,nullptr);return 0;
 case WM_SIZE:if(Canvas){int width=LOWORD(l),height=HIWORD(l);MoveWindow(Canvas,352,108,(std::max)(320,width-376),(std::max)(200,height-282),TRUE);MoveWindow(KeyLabel,24,height-148,(std::max)(320,width-48),24,TRUE);MoveWindow(Status,24,height-64,(std::max)(320,width-48),58,TRUE);MoveWindow(Key,24,height-114,(std::max)(200,width-285),34,TRUE);MoveWindow(Activate,width-237,height-114,213,34,TRUE);}return 0;
 case WM_GETMINMAXINFO:{auto p=reinterpret_cast<MINMAXINFO*>(l);p->ptMinTrackSize={1060,810};return 0;}
 case WM_TIMER:
#ifdef TITAN_OFFLINE_PREVIEW
  if(GetTickCount64()-PreviewStarted>10000){PostMessageW(h,WM_CLOSE,0,0);return 0;}
#endif
  if(HostMode&&!Preview&&!Closing){auto s=Host.Status();if(s!="Stopped"&&s!=LastHostStatus){LastHostStatus=s;EnableWindow(Allow,FALSE);Note(Widen(s));}}return 0;
 case WM_COMMAND:if(HIWORD(w)==BN_CLICKED){switch(LOWORD(w)){case 1:if(!Busy)Mode(true);break;case 2:if(!Busy)Mode(false);break;case 3:StartSession();break;case 4:StopSessions();break;case 5:CopyPair();break;case 6:Verification();break;case 12:if(!HostMode&&SendMessageW(Allow,BM_GETCHECK,0,0)!=BST_CHECKED){ViewerStop=true;Viewer.Stop();Note(L"Control disabled; session disconnected to release held inputs. Reconnect to view again.");}break;}}return 0;
 case WM_APP+1:FramePending=false;InvalidateRect(Canvas,nullptr,FALSE);return 0;
 case WM_APP+2:{std::unique_ptr<std::string> value(reinterpret_cast<std::string*>(l));Note(Widen(*value));return 0;}
 case WM_APP+4:if(Busy){if(Action.joinable())Action.join();Busy=false;Buttons();}SetWindowTextW(Status,L"Operation failed due to a local resource error.");if(Closing)DestroyWindow(h);return 0;
 case WM_APP+3:{std::unique_ptr<std::string> value(reinterpret_cast<std::string*>(l));if(Action.joinable())Action.join();Busy=false;Buttons();Note(value->empty()?L"Full Remote Workstation use activated until Windows restarts.":Widen(*value));if(Closing)DestroyWindow(h);return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORBTN:SetTextColor(reinterpret_cast<HDC>(w),White);SetBkColor(reinterpret_cast<HDC>(w),Panel);return reinterpret_cast<LRESULT>(PanelBrush);
 case WM_DRAWITEM:{auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);HBRUSH b=CreateSolidBrush((d->itemState&ODS_DISABLED)?Panel:Blue);FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,White);SelectObject(d->hDC,Font);wchar_t t[100]{};GetWindowTextW(d->hwndItem,t,100);DrawTextW(d->hDC,t,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);if(d->itemState&ODS_FOCUS)DrawFocusRect(d->hDC,&d->rcItem);return TRUE;}
 case WM_SETCURSOR:{HWND target=reinterpret_cast<HWND>(w);wchar_t type[20]{};GetClassNameW(target,type,20);if(!_wcsicmp(type,L"BUTTON")){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;}
 case WM_CLOSE:Closing=true;StopSessions();if(Busy){Note(L"Finishing the bounded verification request before closing...");return 0;}DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);StopSessions();if(Action.joinable())Action.join();DeleteObject(Font);DeleteObject(Title);DeleteObject(Mono);{MSG pending{};while(PeekMessageW(&pending,h,WM_APP+2,WM_APP+3,PM_REMOVE)){delete reinterpret_cast<std::string*>(pending.lParam);}}PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){try{return ProcImpl(h,m,w,l);}catch(...){SetWindowTextW(Status,L"Operation could not complete due to a local resource error.");return m==WM_CREATE?-1:0;}}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){Preview=command&&wcsstr(command,L"--preview");
#ifdef TITAN_OFFLINE_PREVIEW
Preview=true;
#endif
SetProcessDPIAware();PanelBrush=CreateSolidBrush(Panel);BackgroundBrush=CreateSolidBrush(Black);WNDCLASSW canvas{};canvas.lpfnWndProc=CanvasProc;canvas.hInstance=instance;canvas.lpszClassName=L"TitanRemoteCanvas";canvas.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&canvas);WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=instance;c.lpszClassName=L"TitanRemoteWorkstation";c.hbrBackground=PanelBrush;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);auto h=CreateWindowW(c.lpszClassName,L"Remote Workstation SDK",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1080,830,nullptr,nullptr,instance,nullptr);if(!h)return 1;ShowWindow(h,show);MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){if(!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}DeleteObject(PanelBrush);DeleteObject(BackgroundBrush);return 0;}
