#include "caption_resources.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace cc=dango::captions;
void check(bool b){if(!b)throw std::runtime_error("Caption resource regression");}
void put(cc::Bytes& b,size_t at,uint32_t v,unsigned n=4){if(b.size()<at+n)b.resize(at+n);for(unsigned i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));}
cc::Bytes caption(uint32_t hash,const std::u16string& text){cc::Bytes b(36+512);put(b,0,0x44434356);put(b,4,1);put(b,8,1);put(b,12,512);put(b,16,1);put(b,20,36);put(b,24,hash);put(b,28,0);put(b,32,0,2);put(b,34,uint32_t((text.size()+1)*2),2);for(size_t i=0;i<text.size();++i)put(b,36+i*2,text[i],2);return b;}
void str(cc::Bytes& b,const std::string& text){b.insert(b.end(),text.begin(),text.end());b.push_back(0);}
cc::Bytes vpk(const cc::Bytes& payload,unsigned version=1,unsigned archive=0x7fff,unsigned preload=8){cc::Bytes tree;str(tree,"dat");str(tree,"resource");str(tree,"subtitles_schinese");size_t p=tree.size();tree.resize(p+18);put(tree,p+4,preload,2);put(tree,p+6,archive,2);put(tree,p+8,0);put(tree,p+12,uint32_t(payload.size()-preload));put(tree,p+16,0xffff,2);tree.insert(tree.end(),payload.begin(),payload.begin()+preload);tree.insert(tree.end(),3,0);unsigned h=version==1?12:28;cc::Bytes out(h);put(out,0,0x55aa1234);put(out,4,version);put(out,8,uint32_t(tree.size()));out.insert(out.end(),tree.begin(),tree.end());if(archive==0x7fff)out.insert(out.end(),payload.begin()+preload,payload.end());return out;}
void write(const std::filesystem::path& p,const cc::Bytes& bytes){std::filesystem::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(f));}
int main(){
 auto data=caption(42,u"Zoey: \u88c5\u5f39 \U0001f600");std::map<uint32_t,std::string> found;check(cc::decode(data,found)&&found.at(42)==u8"Zoey: 装弹 😀");auto saved=found;
 auto bad=data;put(bad,28,1);check(!cc::decode(bad,found)&&found==saved);bad=data;put(bad,20,0xffffffff);check(!cc::decode(bad,found));bad=data;bad.resize(40);check(!cc::decode(bad,found));bad=data;put(bad,4,2);check(!cc::decode(bad,found));
 auto root=std::filesystem::temp_directory_path()/("dango-captions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 auto index=root/"left4dead2"/"pak01_dir.vpk";auto packed=vpk(data);write(index,packed);std::vector<cc::Entry> entries;uint32_t inlineAt=0;check(cc::vpkIndex(packed,entries,inlineAt)&&entries.size()==1&&entries[0].name=="resource/subtitles_schinese.dat");check(cc::packed(index,entries[0],inlineAt)==data);
 auto dictionary=cc::load(root,root/"overrides","schinese");check(dictionary.text.at(42)==u8"Zoey: 装弹 😀"&&!dictionary.sources.empty());
 write(root/"overrides"/"subtitles_schinese.dat",caption(42,u"Zoey: override"));write(root/"left4dead2"/"resource"/"subtitles_english.dat",caption(43,u"Nick: fallback"));dictionary=cc::load(root,root/"overrides","schinese");check(dictionary.text.at(42)=="Zoey: override"&&dictionary.text.at(43)=="Nick: fallback");
 auto external=vpk(data,2,0,8);write(index,external);cc::Bytes chunk(data.begin()+8,data.end());write(root/"left4dead2"/"pak01_000.vpk",chunk);entries.clear();check(cc::vpkIndex(external,entries,inlineAt)&&cc::packed(index,entries[0],inlineAt)==data);
 external.resize(20);check(!cc::vpkIndex(external,entries,inlineAt));std::filesystem::remove_all(root);
 std::cout<<"Passed: VCCD bounds/UTF-16, VPK v1/v2 inline+external/preload, dictionary overrides and English fallback\n";
}
