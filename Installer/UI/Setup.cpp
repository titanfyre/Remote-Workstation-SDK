#include "Setup.h"
#include <shlobj.h>
#include <sddl.h>
#include <objbase.h>
#include <filesystem>
#include <array>
#include <stdexcept>
#include <set>
namespace fs=std::filesystem;
namespace {
constexpr wchar_t Marker[]=L"OWNED-RemoteWorkstation.txt";
constexpr char Identity[]="Titan Remote Workstation SDK application 1\n";
constexpr std::array<const wchar_t*,3> Files{L"RemoteWorkstation.exe",L"RemoteWorkstationCLI.exe",L"RemoteWorkstationExample.exe"};
void Require(bool ok){if(!ok){auto e=GetLastError();throw std::system_error(static_cast<int>(e?e:ERROR_GEN_FAILURE),std::system_category());}}
void Safe(const fs::path& path){fs::path p;for(auto& part:path){p/=part;DWORD a=GetFileAttributesW(p.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Refuse redirected installation directory");}}
fs::path Root(){wchar_t directory[MAX_PATH]{};Require(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_PROGRAM_FILES,nullptr,SHGFP_TYPE_CURRENT,directory)));return fs::path(directory)/L"TitanCSA"/L"RemoteWorkstation";}
std::string MarkerText(const fs::path& path){HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h==INVALID_HANDLE_VALUE)return {};char bytes[sizeof(Identity)]{};DWORD count=0;BOOL ok=ReadFile(h,bytes,sizeof(bytes),&count,nullptr);CloseHandle(h);return ok?std::string(bytes,count):std::string{};}
bool Owned(const fs::path& root){if(GetFileAttributesW(root.c_str())==INVALID_FILE_ATTRIBUTES)return false;Safe(root);if(MarkerText(root/Marker)!=Identity)throw std::runtime_error("Installation marker differs");std::set<std::wstring> expected{Marker};for(auto file:Files)expected.insert(file);for(auto& file:fs::directory_iterator(root)){auto attributes=GetFileAttributesW(file.path().c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))||!expected.count(file.path().filename().wstring()))throw std::runtime_error("Unexpected installation content; preserve this folder and inspect it");}return true;}
void RemoveOwned(const fs::path& root){if(!Owned(root))return;for(auto name:Files){auto p=root/name;if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES)Require(DeleteFileW(p.c_str())!=FALSE);}Require(DeleteFileW((root/Marker).c_str())!=FALSE);Require(RemoveDirectoryW(root.c_str())!=FALSE);}
void NewFile(const fs::path& path,const void* data,DWORD length){HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Require(h!=INVALID_HANDLE_VALUE);DWORD written=0;BOOL ok=WriteFile(h,data,length,&written,nullptr);DWORD e=GetLastError();if(ok&&written==length)ok=FlushFileBuffers(h);if(!ok)e=GetLastError();CloseHandle(h);if(!ok||written!=length){SetLastError(e);Require(false);}}
}
namespace AppSetup {
std::wstring Location(){return Root().wstring();}
bool Installed()noexcept{try{return Owned(Root())&&GetFileAttributesW((Root()/Files[0]).c_str())!=INVALID_FILE_ATTRIBUTES;}catch(...){return false;}}
RuntimeSetup::Result Execute(bool install)noexcept{
 HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Global\\TitanRemoteWorkstationSetup");if(!mutex)return {GetLastError(),false,L"Could not open setup lock."};DWORD wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);return {ERROR_BUSY,false,L"Another Remote Workstation installation is already running."};}
 struct Lock{HANDLE h;~Lock(){ReleaseMutex(h);CloseHandle(h);}} lock{mutex};
 fs::path stage,backup;bool movedOld=false,movedNew=false;
 try{
  const auto target=Root();Safe(target);const bool existed=Owned(target);
  if(!install){RemoveOwned(target);return {0,false,L"Remote Workstation application removed. The shared runtime was preserved for the other SDKs. No firewall or certificate trust setting was changed."};}
  // Validate all application resources before any installation work.
  std::array<std::pair<const void*,DWORD>,3> images{};
  for(size_t i=0;i<Files.size();++i){HRSRC r=FindResourceW(nullptr,MAKEINTRESOURCEW(201+static_cast<int>(i)),RT_RCDATA);Require(r!=nullptr);DWORD n=SizeofResource(nullptr,r);auto bytes=LockResource(LoadResource(nullptr,r));Require(bytes&&n>4096&&n<32*1024*1024&&static_cast<const char*>(bytes)[0]=='M'&&static_cast<const char*>(bytes)[1]=='Z');images[i]={bytes,n};}
  auto runtime=RuntimeSetup::Install();if(runtime.error)return runtime;
  GUID id{};Require(SUCCEEDED(CoCreateGuid(&id)));wchar_t token[40]{};Require(StringFromGUID2(id,token,40)>0);auto parent=target.parent_path();fs::create_directories(parent);Safe(parent);stage=parent/(std::wstring(L".RemoteWorkstation-stage-")+token);backup=parent/(std::wstring(L".RemoteWorkstation-backup-")+token);
  PSECURITY_DESCRIPTOR descriptor=nullptr;Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;BU)",SDDL_REVISION_1,&descriptor,nullptr)!=FALSE);SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};BOOL made=CreateDirectoryW(stage.c_str(),&security);DWORD error=GetLastError();LocalFree(descriptor);if(!made){SetLastError(error);Require(false);}
  // Marker is written first so only this attempt's known files are removable.
  NewFile(stage/Marker,Identity,static_cast<DWORD>(sizeof(Identity)-1));for(size_t i=0;i<Files.size();++i)NewFile(stage/Files[i],images[i].first,images[i].second);
  if(existed){Require(MoveFileExW(target.c_str(),backup.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE);movedOld=true;}
  Require(MoveFileExW(stage.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE);movedNew=true;
  if(movedOld){RemoveOwned(backup);movedOld=false;}
  return {0,runtime.reboot,L"Remote Workstation installed in "+target.wstring()+L". No listener starts automatically. Keep this setup to uninstall the application; the shared runtime stays installed."};
 }catch(const std::system_error& error){try{if(!movedNew&&movedOld)MoveFileExW(backup.c_str(),Root().c_str(),MOVEFILE_WRITE_THROUGH);if(!stage.empty()&&GetFileAttributesW(stage.c_str())!=INVALID_FILE_ATTRIBUTES)RemoveOwned(stage);}catch(...){}return {static_cast<DWORD>(error.code().value()),false,L"Application setup failed (Windows error "+std::to_wstring(error.code().value())+L"). Close the application and retry. The shared runtime may already be installed."};}
 catch(...){try{if(!movedNew&&movedOld)MoveFileExW(backup.c_str(),Root().c_str(),MOVEFILE_WRITE_THROUGH);if(!stage.empty()&&GetFileAttributesW(stage.c_str())!=INVALID_FILE_ATTRIBUTES)RemoveOwned(stage);}catch(...){}return {ERROR_INVALID_DATA,false,L"Setup refused an unexpected path, ownership marker or payload. Existing unknown files were preserved."};}
}
}
