#include "../SDK/RemoteWorkstation.h"
#include <iostream>

int main(){
 std::cout<<"View ten frames from an explicitly paired host. No input is sent.\nHost IPv4: ";
 std::string address,pair;std::getline(std::cin,address);
 std::cout<<"Host pairing code: ";std::getline(std::cin,pair);
 RemoteWorkstation::Viewer viewer;
 auto error=viewer.Connect(address,48380,pair);
 if(!error.empty()){std::cerr<<error<<"\n";return 1;}
 try{for(int i=0;i<10;++i){auto frame=viewer.Receive();std::cout<<frame.width<<" x "<<frame.height<<" BGRA frame\n";}}
 catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
