#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
namespace dango::captions {
using Bytes=std::vector<uint8_t>;
inline uint32_t u32(const Bytes& b,size_t n){return uint32_t(b[n])|(uint32_t(b[n+1])<<8)|(uint32_t(b[n+2])<<16)|(uint32_t(b[n+3])<<24);}
inline uint16_t u16(const Bytes& b,size_t n){return uint16_t(b[n]|(uint16_t(b[n+1])<<8));}
inline bool range(uint64_t at,uint64_t length,uint64_t size){return at<=size&&length<=size-at;}
inline Bytes readRange(const std::filesystem::path& path,uint64_t offset,size_t count){
 if(count>32*1024*1024)return {};std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return {};auto size=f.tellg();if(size<0||!range(offset,count,uint64_t(size)))return {};Bytes b(count);f.seekg(std::streamoff(offset));if(count&&!f.read(reinterpret_cast<char*>(b.data()),std::streamsize(count)))return {};return b;
}
inline Bytes readFile(const std::filesystem::path& path){std::error_code ec;auto n=std::filesystem::file_size(path,ec);return !ec&&n<=32*1024*1024?readRange(path,0,size_t(n)):Bytes{};}
inline std::string utf16(const Bytes& b,size_t at,size_t count){
 if(count%2||!range(at,count,b.size()))return {};std::string out;
 for(size_t end=at+count;at<end;at+=2){uint32_t c=u16(b,at);if(!c)return out;if(c>=0xd800&&c<=0xdbff){if(at+3>=end)return {};uint32_t low=u16(b,at+2);if(low<0xdc00||low>0xdfff)return {};c=0x10000+((c-0xd800)<<10)+low-0xdc00;at+=2;}else if(c>=0xdc00&&c<=0xdfff)return {};
 if(c<128)out+=char(c);else if(c<2048){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}else if(c<65536){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}}
 return {};
}
// VCCD v1: 24-byte header, 12-byte hash/block/offset/length directory.
inline bool decode(const Bytes& b,std::map<uint32_t,std::string>& out){
 if(b.size()<24||u32(b,0)!=0x44434356||u32(b,4)!=1)return false;auto blocks=u32(b,8),blockSize=u32(b,12),entries=u32(b,16),data=u32(b,20);
 if(!blocks||blocks>16384||blockSize<2||blockSize>65536||entries>65536||!range(24,uint64_t(entries)*12,b.size())||data<24+uint64_t(entries)*12||!range(data,uint64_t(blocks)*blockSize,b.size()))return false;
 uint64_t textBytes=0;for(const auto& item:out)textBytes+=item.second.size();
 std::map<uint32_t,std::string> next;for(uint32_t i=0;i<entries;++i){size_t p=24+size_t(i)*12;auto hash=u32(b,p),block=u32(b,p+4);auto offset=u16(b,p+8),length=u16(b,p+10);
 if(block>=blocks||!length||length>8192||!range(offset,length,blockSize))return false;if(out.count(hash)||next.count(hash))continue;auto text=utf16(b,size_t(data)+size_t(block)*blockSize+offset,length);if(text.empty())continue;if(text.size()>4096||textBytes+text.size()>32*1024*1024)return false;textBytes+=text.size();next.emplace(hash,std::move(text));}
 if(out.size()+next.size()>262144)return false;out.merge(next);return true;
}
struct Entry {std::string name;uint16_t archive=0;uint32_t offset=0,length=0;Bytes preload;};
inline bool vpkIndex(const Bytes& b,std::vector<Entry>& out,uint32_t& dataOffset){
 if(b.size()<12||u32(b,0)!=0x55aa1234)return false;auto version=u32(b,4),tree=u32(b,8);uint32_t header=version==1?12:version==2?28:0;if(!header||b.size()<header||tree>16*1024*1024||!range(header,tree,b.size()))return false;dataOffset=header+tree;size_t p=header,end=dataOffset;
 auto text=[&](std::string& s){s.clear();while(p<end&&b[p]){if(s.size()>=512)return false;s+=char(b[p++]);}if(p>=end)return false;++p;return true;};
 std::vector<Entry> next;std::string ext,path,file;
 for(;;){if(!text(ext))return false;if(ext.empty())break;
 for(;;){if(!text(path))return false;if(path.empty())break;
 for(;;){if(!text(file))return false;if(file.empty())break;if(!range(p,18,end))return false;auto preload=u16(b,p+4);Entry entry;entry.archive=u16(b,p+6);entry.offset=u32(b,p+8);entry.length=u32(b,p+12);if(u16(b,p+16)!=0xffff)return false;p+=18;if(!range(p,preload,end))return false;
 entry.name=(path==" "?"":path+"/")+file+(ext==" "?"":"."+ext);if(entry.name.rfind("resource/",0)==0&&(entry.name.find("resource/subtitles_")==0||entry.name.find("resource/closecaption_")==0)&&ext=="dat"){if(next.size()>=256)return false;entry.preload.assign(b.begin()+p,b.begin()+p+preload);next.push_back(std::move(entry));}p+=preload;
 }}}
 out=std::move(next);return true;
}
inline Bytes packed(const std::filesystem::path& index,const Entry& e,uint32_t inlineOffset){
 if(e.archive!=0x7fff&&e.archive>9999)return {};std::filesystem::path archive=index;uint64_t offset=e.offset;
 if(e.archive==0x7fff)offset+=inlineOffset;else{auto stem=index.stem().string();if(stem.size()<4||stem.substr(stem.size()-4)!="_dir")return {};auto number=std::to_string(e.archive);while(number.size()<3)number="0"+number;archive=index.parent_path()/(stem.substr(0,stem.size()-4)+"_"+number+".vpk");}
 auto bytes=readRange(archive,offset,e.length);if(bytes.size()!=e.length)return {};Bytes result=e.preload;result.insert(result.end(),bytes.begin(),bytes.end());return result;
}
struct Dictionary {std::map<uint32_t,std::string> text;std::vector<std::string> sources,errors;};
inline Dictionary load(const std::filesystem::path& game,const std::filesystem::path& overrides,const std::string& language){
 Dictionary out;std::vector<std::string> languages{language};if(language!="english")languages.push_back("english");
 // Overrides first, then Valve's DLCs before the base game. Earlier hashes win.
 for(const auto& lang:languages){
 for(const char* kind:{"subtitles_","closecaption_"}){auto path=overrides/(std::string(kind)+lang+".dat");auto data=readFile(path);if(!data.empty()){if(decode(data,out.text))out.sources.push_back(path.u8string());else out.errors.push_back("Invalid VCCD: "+path.u8string());}}
 for(const char* folder:{"left4dead2_dlc3","left4dead2_dlc2","left4dead2_dlc1","left4dead2"}){
 auto root=game/folder;for(const char* kind:{"subtitles_","closecaption_"}){auto path=root/"resource"/(std::string(kind)+lang+".dat");auto data=readFile(path);if(!data.empty()){if(decode(data,out.text))out.sources.push_back(path.u8string());else out.errors.push_back("Invalid VCCD: "+path.u8string());}}
 auto index=root/"pak01_dir.vpk";auto header=readRange(index,0,12);if(header.size()!=12)continue;auto n=u32(header,8);if(n>16*1024*1024){out.errors.push_back("VPK tree too large: "+index.u8string());continue;}auto bytes=readRange(index,0,size_t(n)+(u32(header,4)==2?28:12));std::vector<Entry> entries;uint32_t offset=0;if(!vpkIndex(bytes,entries,offset)){out.errors.push_back("Invalid VPK index: "+index.u8string());continue;}
 for(const char* kind:{"subtitles_","closecaption_"})for(const auto& entry:entries)if(entry.name=="resource/"+std::string(kind)+lang+".dat"){auto data=packed(index,entry,offset);if(decode(data,out.text))out.sources.push_back(index.u8string()+":"+entry.name);else out.errors.push_back("Unreadable/invalid caption entry: "+entry.name);}
 }}return out;
}
}
