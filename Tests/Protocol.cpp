#include "../SDK/RemoteWorkstation.h"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
int main(){
 try{using namespace RemoteWorkstation;unsigned checks=0;
  for(auto size:{std::pair{16u,16u},std::pair{960u,540u},std::pair{1280u,720u}}){Frame original;original.width=size.first;original.height=size.second;original.sequence=12345;original.bgra.resize(static_cast<size_t>(original.width)*original.height*4);std::mt19937 generator(17);for(auto& byte:original.bgra)byte=static_cast<uint8_t>(generator());auto encoded=EncodeFrame(original);auto decoded=DecodeFrame(encoded);if(decoded.bgra!=original.bgra||decoded.width!=original.width||decoded.height!=original.height||decoded.sequence!=original.sequence)throw std::runtime_error("Lossless frame codec roundtrip differs");++checks;
   for(unsigned field:{0u,4u,8u,12u}){auto bad=encoded;uint32_t invalid=field==0?0:field==4?721:field==8?1:1;std::memcpy(bad.data()+field,&invalid,4);bool refused=false;try{DecodeFrame(bad);}catch(const std::exception&){refused=true;}if(!refused)throw std::runtime_error("Invalid frame metadata accepted");++checks;}
   auto truncated=encoded;truncated.resize(25);bool refused=false;try{DecodeFrame(truncated);}catch(const std::exception&){refused=true;}if(!refused)throw std::runtime_error("Truncated frame accepted");++checks;
  }
  for(int i=-10000;i<90000;++i){Input input{InputKind::Pointer,i,100,0,0};if(ValidInput(input)!=(i>=0&&i<=65535))throw std::runtime_error("Pointer normalization boundary differs");++checks;}
  for(unsigned code=0;code<1024;++code){if(ValidInput(Input{InputKind::Key,0,0,code,1})!=(code>=1&&code<=255))throw std::runtime_error("Virtual-key range differs");if(ValidInput(Input{InputKind::Button,0,0,code,0})!=(code>=1&&code<=3))throw std::runtime_error("Button range differs");checks+=2;}
  for(int delta=-2000;delta<=2000;++delta){if(ValidInput(Input{InputKind::Wheel,0,delta,0,0})!=(delta>=-1200&&delta<=1200))throw std::runtime_error("Signed wheel bound differs");++checks;}
  for(unsigned kind=5;kind<100;++kind){if(ValidInput(Input{static_cast<InputKind>(kind),0,0,1,1}))throw std::runtime_error("Unknown input kind accepted");++checks;}
  if(ValidInput(Input{InputKind::Key,0,0,65,2})||ValidInput(Input{InputKind::Pointer,1,1,1,0}))throw std::runtime_error("Reserved fields accepted");checks+=2;
  std::cout<<checks<<" independent frame/input boundary checks passed. No desktop capture, input injection or license Begin was called.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
