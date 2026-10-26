#include "../SDK/RemoteWorkstation.h"
#include <iostream>
#include <string>
int main(int argc,char** argv){
 try{
  if(argc==2&&std::string(argv[1])=="--preview"){std::cout<<"Remote Workstation SDK offline preview. No driver access, capture, input injection or listener.\n";return 0;}
  if(argc==3&&std::string(argv[1])=="verify"){auto error=RemoteWorkstation::Verification(argv[2]);std::cout<<(error.empty()?"Full Remote Workstation use is activated until Windows restarts.":error)<<"\n";return error.empty()?0:1;}
  if(argc==3&&std::string(argv[1])=="host"){
   RemoteWorkstation::Host host;RemoteWorkstation::HostOptions options;options.address=argv[2];auto error=host.Start(options);if(!error.empty())throw std::runtime_error(error);
   std::cout<<"Visible view-only host started on "<<options.address<<":"<<host.Port()<<".\nShare this one-session pairing code privately with the authorized viewer:\n"<<host.PairingCode()<<"\nPress Enter to stop. No firewall rule was added.\n";std::string line;std::getline(std::cin,line);host.Stop();return 0;
  }
  if(argc==4&&std::string(argv[1])=="view"){
   RemoteWorkstation::Viewer viewer;auto error=viewer.Connect(argv[2],48380,argv[3]);if(!error.empty())throw std::runtime_error(error);for(unsigned i=0;i<10;++i){auto frame=viewer.Receive();std::cout<<"Frame "<<frame.sequence<<": "<<frame.width<<" x "<<frame.height<<", "<<frame.bgra.size()<<" decoded BGRA bytes.\n";}viewer.Stop();return 0;
  }
  std::cout<<"Remote Workstation SDK\n\nRemoteWorkstationCLI.exe host 192.168.1.10\nRemoteWorkstationCLI.exe view 192.168.1.10 FINGERPRINT:SESSION_SECRET\nRemoteWorkstationCLI.exe verify PRODUCT_KEY\nRemoteWorkstationCLI.exe --preview\n\nRun as administrator after installing the signed shared runtime. The UI displays received frames. Host mode above is view-only.\n";return 2;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
