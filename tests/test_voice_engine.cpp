#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
extern "C" {
#include "voice_engine.h"
}
#include "external_sender.h"
struct Sink {std::vector<unsigned char> key,commit;};
int send(void *u,int binary,const unsigned char *p,size_t n){auto &s=*(Sink*)u;if(binary&&n){if(p[0]==26)s.key.assign(p+1,p+n);if(p[0]==28)s.commit.assign(p+1,p+n);}return 1;}
void binary(DnxVoiceEngine *e,int op,std::vector<unsigned char> p,int transition=-1){std::vector<unsigned char>b={0,1,(unsigned char)op};if(transition>=0){b.push_back(transition>>8);b.push_back(transition);}b.insert(b.end(),p.begin(),p.end());assert(dnx_voice_signal_binary(e,b.data(),b.size())==1);}
void run(const char *mode){Sink a,b;auto A=dnx_voice_create("123456789012345678","323456789012345678",send,&a);auto B=dnx_voice_create("223456789012345678","323456789012345678",send,&b);assert(A&&B);std::string keys;for(int i=0;i<32;i++)keys+=(i?",":"")+std::to_string(i);std::string desc=std::string("{\"op\":4,\"d\":{\"mode\":\"")+mode+"\",\"dave_protocol_version\":1,\"secret_key\":["+keys+"]}}";
assert(dnx_voice_signal_json(A,desc.c_str())==1);assert(dnx_voice_signal_json(B,desc.c_str())==1);
const char *clients="{\"op\":11,\"d\":{\"user_ids\":[\"123456789012345678\",\"223456789012345678\"]}}";assert(dnx_voice_signal_json(A,clients)==1);assert(dnx_voice_signal_json(B,clients)==1);
discord::dave::test::ExternalSender server(1,323456789012345678ULL);binary(A,25,server.GetMarshalledExternalSender());binary(B,25,server.GetMarshalledExternalSender());binary(A,27,server.ProposeAdd(0,b.key));assert(!a.commit.empty());auto pair=server.SplitCommitWelcome(a.commit);binary(A,29,pair.first,1);binary(B,30,pair.second,1);
int16_t pcm[960];for(int i=0;i<960;i++)pcm[i]=(int16_t)(sin(i*0.06)*10000);unsigned char packet[2048];dnx_voice_ssrc(A,4000000000U);assert(dnx_voice_encode(A,pcm,packet)==0);assert(dnx_voice_signal_json(A,"{\"op\":22,\"d\":{\"transition_id\":1}}")==1);assert(dnx_voice_signal_json(B,"{\"op\":22,\"d\":{\"transition_id\":1}}")==1);assert(dnx_voice_signal_json(B,"{\"op\":5,\"d\":{\"user_id\":\"123456789012345678\",\"ssrc\":4000000000}}")==1);
long long energy=0;for(int i=0;i<8;i++){int n=dnx_voice_encode(A,pcm,packet);assert(n>40);auto bad=std::vector<unsigned char>(packet,packet+n);bad[20]^=1;assert(!dnx_voice_receive(B,bad.data(),bad.size()));assert(dnx_voice_receive(B,packet,n)==1);assert(!dnx_voice_receive(B,packet,n));int16_t out[1920];dnx_voice_mix(B,out);for(auto x:out)energy+=(long long)x*x;}assert(energy>1000000);assert(dnx_voice_signal_json(A,"{\"op\":21,\"d\":{\"protocol_version\":0,\"transition_id\":2}}")==-1);dnx_voice_ssrc(B,3000000000U);assert(dnx_voice_signal_json(A,"{\"op\":5,\"d\":{\"user_id\":\"223456789012345678\",\"ssrc\":3000000000}}")==1);int reverse=dnx_voice_encode(B,pcm,packet);assert(reverse>40&&dnx_voice_receive(A,packet,reverse)==1);
assert(dnx_voice_signal_json(B,"{\"op\":13,\"d\":{\"user_id\":\"123456789012345678\"}}")==1);int removed=dnx_voice_encode(A,pcm,packet);assert(removed>40&&!dnx_voice_receive(B,packet,removed));
dnx_voice_destroy(A);dnx_voice_destroy(B);printf("PASS %s: real MLS membership, transition gating, Opus+E2EE+transport roundtrip, tamper and replay rejection\n",mode);}
int main(){run("aead_aes256_gcm_rtpsize");run("aead_xchacha20_poly1305_rtpsize");}
