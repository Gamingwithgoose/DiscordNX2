// SPDX-License-Identifier: MIT
/* Browser Research v0.1: read installed browser content; write only a fresh SD run. */
#include <switch.h>
#include <switch/services/fsldr.h>
#include <switch/services/ncm.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include "copy_core.h"

#define FILE_LIMIT (256ULL*1024*1024)
#define RUN_LIMIT (512ULL*1024*1024)
#define FILE_COUNT_LIMIT 4096
#define DEPTH_LIMIT 12
static const struct {u64 id;const char *name;NcmContentType type;} targets[]={
 {0x0400000000001043ULL,"openweb",NcmContentType_Program},
 {0x0400000000001042ULL,"systemweb",NcmContentType_Program},
 {0x040000000000100fULL,"offlineweb0",NcmContentType_Program},
 {0x040000000000104bULL,"offlineweb3",NcmContentType_Program},
 {0x0400000000000803ULL,"BrowserDll",NcmContentType_Data},
 {0x040000000000085dULL,"BrowserCoreDll",NcmContentType_Program}
};
static FsFileSystem sd;static int sd_ready,exit_requested,cancelled,errors;
static NcmContentMetaDatabase database;static NcmContentStorage content_storage;
static int db_ready,cs_ready,ldr_ready;
static PadState pad;static unsigned char copy_buffer[65536];
static char run_path[128],report_data[131072];static size_t report_used;static int report_full;
static uint64_t attempted_bytes,complete_bytes;static unsigned files_seen,files_complete;
static u64 last_draw;static char current_file[256];

static void record(const char *fmt,...){
 if(report_full)return;
 size_t capacity=sizeof(report_data)-256-report_used;
 va_list args;va_start(args,fmt);int n=vsnprintf(report_data+report_used,capacity,fmt,args);va_end(args);
 if(n>0&&(size_t)n<capacity)report_used+=(size_t)n;
 else if(n>0){
  report_used+=capacity-1;report_full=1;
  const char *note="\nREPORT TRUNCATED: export may continue; this report is not a complete file inventory.\n";
  size_t length=strlen(note);memcpy(report_data+report_used,note,length+1);report_used+=length;
 }
}
static void checkpoint(void){
 if(!sd_ready)return;
 char path[180];snprintf(path,sizeof(path),"%s/report.txt",run_path);
 FsFile out={0};Result rc=fsFsOpenFile(&sd,path,FsOpenMode_Write,&out);
 if(R_FAILED(rc)){rc=fsFsCreateFile(&sd,path,(s64)report_used,0);if(R_SUCCEEDED(rc))rc=fsFsOpenFile(&sd,path,FsOpenMode_Write,&out);}
 if(R_SUCCEEDED(rc)){rc=fsFileSetSize(&out,(s64)report_used);if(R_SUCCEEDED(rc))rc=fsFileWrite(&out,0,report_data,report_used,FsWriteOption_Flush);fsFileClose(&out);}
 if(R_FAILED(rc)){printf("Report write failed: %08x\n",rc);errors++;}
}
static int poll_cancel(void *unused){
 (void)unused;padUpdate(&pad);u64 held=padGetButtons(&pad);
 if(held&HidNpadButton_Plus)exit_requested=1;
 if(held&(HidNpadButton_B|HidNpadButton_Plus))cancelled=1;
 u64 now=armGetSystemTick();if(now-last_draw>armNsToTicks(100000000ULL)){
  printf("\x1b[2J\x1b[HBROWSER RESEARCH v0.1\n\nReading installed browser content\n%s\n\nCompleted files: %u   Errors: %d\nCompleted bytes: %" PRIu64 "\n\nB: cancel export   PLUS: cancel and exit\n\nSD:%s\n",current_file,files_complete,errors,complete_bytes,run_path);consoleUpdate(NULL);last_draw=now;
 }
 return cancelled;
}
static int new_run(void){
 (void)fsFsCreateDirectory(&sd,"/switch");(void)fsFsCreateDirectory(&sd,"/switch/DiscordNX2-Browser");
 snprintf(run_path,sizeof(run_path),"/switch/DiscordNX2-Browser/run-%016" PRIx64,(uint64_t)armGetSystemTick());
 Result rc=fsFsCreateDirectory(&sd,run_path);if(R_FAILED(rc)){printf("Could not create fresh output folder: %08x\n",rc);return 0;}
 report_used=0;report_full=0;attempted_bytes=complete_bytes=0;files_seen=files_complete=0;errors=cancelled=0;current_file[0]=0;
 SetSysFirmwareVersion firmware={0};Result vrc=setsysInitialize();if(R_SUCCEEDED(vrc)){vrc=setsysGetFirmwareVersion(&firmware);setsysExit();}
 record("DiscordNX2 browser research v0.1\nSources are opened read-only. SD output is private to this fresh run.\nfirmware_result=%08x firmware=%u.%u.%u\n",vrc,firmware.major,firmware.minor,firmware.micro);
 record("Not a browser unlock or a successful launch test. No account tokens, keys or network use.\n");checkpoint();return 1;
}
typedef struct {FsFile source,destination;FsStorage storage;int raw;Result read_rc,write_rc;Sha256Context hash;} Copy;
static int source_read(void *ctx,uint64_t offset,void *buffer,size_t size,size_t *got){
 Copy *copy=ctx;if(copy->raw){copy->read_rc=fsStorageRead(&copy->storage,(s64)offset,buffer,size);*got=R_SUCCEEDED(copy->read_rc)?size:0;}
 else {u64 count=0;copy->read_rc=fsFileRead(&copy->source,(s64)offset,buffer,size,FsReadOption_None,&count);*got=(size_t)count;}
 return R_SUCCEEDED(copy->read_rc);
}
static int destination_write(void *ctx,uint64_t offset,const void *buffer,size_t size){
 Copy *copy=ctx;copy->write_rc=fsFileWrite(&copy->destination,(s64)offset,buffer,size,FsWriteOption_None);
 if(R_SUCCEEDED(copy->write_rc))sha256ContextUpdate(&copy->hash,buffer,size);
 return R_SUCCEEDED(copy->write_rc);
}
static int export_open_source(Copy *copy,const char *destination,s64 size){
 if(size<0||(uint64_t)size>FILE_LIMIT||(uint64_t)size>RUN_LIMIT-attempted_bytes){record("SKIP limit path=%s size=%" PRId64 "\n",destination,(int64_t)size);errors++;return 0;}
 attempted_bytes+=(uint64_t)size;char temporary[FS_MAX_PATH];
 int n=snprintf(temporary,sizeof(temporary),"%s.part",destination);if(n<0||(size_t)n>=sizeof(temporary)){errors++;return 0;}
 Result rc=fsFsCreateFile(&sd,temporary,size,0);if(R_SUCCEEDED(rc))rc=fsFsOpenFile(&sd,temporary,FsOpenMode_Write,&copy->destination);
 if(R_FAILED(rc)){record("SD_CREATE result=%08x path=%s\n",rc,temporary);errors++;checkpoint();return 0;}
 sha256ContextCreate(&copy->hash);snprintf(current_file,sizeof(current_file),"%.255s",destination);checkpoint();uint64_t copied=0;
 int result=browser_copy(copy,source_read,destination_write,poll_cancel,(uint64_t)size,FILE_LIMIT,copy_buffer,sizeof(copy_buffer),&copied);
 if(result==BROWSER_COPY_OK){rc=fsFileFlush(&copy->destination);if(R_FAILED(rc)){copy->write_rc=rc;result=BROWSER_COPY_WRITE;}}
 fsFileClose(&copy->destination);
 if(result==BROWSER_COPY_OK){rc=fsFsRenameFile(&sd,temporary,destination);if(R_FAILED(rc)){copy->write_rc=rc;result=BROWSER_COPY_WRITE;}}
 if(result==BROWSER_COPY_OK){unsigned char hash[32];char hex[65];sha256ContextGetHash(&copy->hash,hash);for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",hash[i]);files_complete++;complete_bytes+=(uint64_t)size;record("COMPLETE bytes=%" PRIu64 " sha256=%s path=%s\n",copied,hex,destination);}
 else {errors++;record("PARTIAL copy=%d read=%08x write=%08x bytes=%" PRIu64 " path=%s\n",result,copy->read_rc,copy->write_rc,copied,temporary);}
 memset(copy_buffer,0,sizeof(copy_buffer));checkpoint();return result==BROWSER_COPY_OK;
}
/* Enumerate bounded names from a source filesystem; never pass it a write mode. */
static void export_tree(FsFileSystem *source,const char *from,const char *to,unsigned depth,int export_files){
 if(cancelled||files_seen>=FILE_COUNT_LIMIT)return;
 if(depth>DEPTH_LIMIT){record("SKIP depth path=%s\n",from);errors++;return;}
 FsDir dir={0};Result rc=fsFsOpenDirectory(source,from,FsDirOpenMode_ReadDirs|FsDirOpenMode_ReadFiles,&dir);
 if(R_FAILED(rc)){record("DIRECTORY result=%08x path=%s\n",rc,from);errors++;return;}
 for(;;){FsDirectoryEntry entry={0};s64 count=0;if(poll_cancel(NULL))break;
  rc=fsDirRead(&dir,&count,1,&entry);if(R_FAILED(rc)){record("DIRECTORY_READ result=%08x\n",rc);errors++;break;}if(!count)break;if(count!=1){errors++;break;}
  if(++files_seen>FILE_COUNT_LIMIT){record("SKIP entry count limit\n");errors++;break;}
  if(!browser_component(entry.name,sizeof(entry.name))){record("SKIP unsafe directory name\n");errors++;continue;}
  char in[FS_MAX_PATH],out[FS_MAX_PATH];int a=snprintf(in,sizeof(in),"%s%s%s",from,strcmp(from,"/")?"/":"",entry.name),b=snprintf(out,sizeof(out),"%s/%s",to,entry.name);
  if(a<0||b<0||(size_t)a>=sizeof(in)||(size_t)b+5>=sizeof(out)){record("SKIP long path\n");errors++;continue;}
  if(entry.type==FsDirEntryType_Dir){if(export_files){rc=fsFsCreateDirectory(&sd,out);if(R_FAILED(rc)){record("SD_DIRECTORY result=%08x\n",rc);errors++;continue;}}export_tree(source,in,out,depth+1,export_files);}
  else if(entry.type==FsDirEntryType_File){record("FILE size=%" PRId64 " path=%s\n",(int64_t)entry.file_size,in);
   if(export_files){Copy copy={0};rc=fsFsOpenFile(source,in,FsOpenMode_Read,&copy.source);if(R_SUCCEEDED(rc)){s64 size=0;rc=fsFileGetSize(&copy.source,&size);if(R_SUCCEEDED(rc))export_open_source(&copy,out,size);else {record("SIZE result=%08x\n",rc);errors++;}fsFileClose(&copy.source);}else {record("SOURCE_OPEN result=%08x\n",rc);errors++;}}
  }else {record("SKIP unknown entry type=%d\n",entry.type);errors++;}
 }
 fsDirClose(&dir);checkpoint();
}
static void inspect_target(unsigned index,int export_files){
 u64 id=targets[index].id;char base[180],destination[220];snprintf(base,sizeof(base),"%s/%016" PRIx64,run_path,(uint64_t)id);
 record("\nTARGET %s id=%016" PRIx64 "\n",targets[index].name,(uint64_t)id);checkpoint();if(poll_cancel(NULL))return;
 if(export_files){Result rc=fsFsCreateDirectory(&sd,base);if(R_FAILED(rc)){record("SD_TARGET_DIRECTORY result=%08x\n",rc);errors++;return;}}
 /* Data-only archives can be read without resolving an ExeFS path. */
 if(targets[index].type==NcmContentType_Data){Copy copy={0};Result rc=fsOpenDataStorageByDataId(&copy.storage,id,NcmStorageId_BuiltInSystem);record("DATA_STORAGE result=%08x\n",rc);
  if(R_SUCCEEDED(rc)){s64 size=0;rc=fsStorageGetSize(&copy.storage,&size);record("DATA_SIZE result=%08x bytes=%" PRId64 "\n",rc,(int64_t)size);if(export_files&&R_SUCCEEDED(rc)){copy.raw=1;snprintf(destination,sizeof(destination),"%s/romfs.bin",base);export_open_source(&copy,destination,size);}fsStorageClose(&copy.storage);}else errors++;
 }
 char content_path[FS_MAX_PATH]={0};NcmContentMetaKey key={0};NcmContentId content={0};
 if(!db_ready||!cs_ready){record("NCM unavailable: code/content mount not attempted\n");errors++;checkpoint();return;}
 Result rc=ncmContentMetaDatabaseGetLatestContentMetaKey(&database,&key,id);record("META result=%08x version=%u type=%u\n",rc,key.version,key.type);
 if(R_SUCCEEDED(rc)){rc=ncmContentMetaDatabaseGetContentIdByType(&database,&content,&key,targets[index].type);record("CONTENT_ID result=%08x\n",rc);}
 if(R_SUCCEEDED(rc)){rc=ncmContentStorageGetPath(&content_storage,content_path,sizeof(content_path),&content);record("CONTENT_PATH result=%08x\n",rc);}
 content_path[sizeof(content_path)-1]=0;if(R_FAILED(rc)||!content_path[0]){errors++;checkpoint();return;}
 if(targets[index].type==NcmContentType_Program){
  if(ldr_ready){FsCodeInfo info={0};FsFileSystem code={0};rc=fsldrOpenCodeFileSystem(&info,id,NcmStorageId_BuiltInSystem,content_path,FsContentAttributes_All,&code);record("CODE_FILESYSTEM result=%08x signed=%u\n",rc,R_SUCCEEDED(rc)?info.is_signed:0);checkpoint();
   if(R_SUCCEEDED(rc)){snprintf(destination,sizeof(destination),"%s/exefs",base);if(export_files)rc=fsFsCreateDirectory(&sd,destination);if(R_SUCCEEDED(rc))export_tree(&code,"/",destination,0,export_files);else {record("SD_EXEFS_DIRECTORY result=%08x\n",rc);errors++;}fsFsClose(&code);}else errors++;
  }else {record("Loader filesystem unavailable\n");errors++;}
 }
 if(cancelled)return;
 FsFileSystem data={0};rc=fsOpenFileSystemWithId(&data,id,FsFileSystemType_ContentData,content_path,FsContentAttributes_All);record("CONTENT_DATA_FILESYSTEM result=%08x\n",rc);checkpoint();
 if(R_SUCCEEDED(rc)){snprintf(destination,sizeof(destination),"%s/romfs",base);if(export_files)rc=fsFsCreateDirectory(&sd,destination);if(R_SUCCEEDED(rc))export_tree(&data,"/",destination,0,export_files);else {record("SD_ROMFS_DIRECTORY result=%08x\n",rc);errors++;}fsFsClose(&data);}else errors++;
}
static void run_probe(int export_files){
 if(!new_run())return;
 record("MODE %s\n",export_files?"EXPORT":"INVENTORY");
 Result rc=ncmInitialize();int ncm_ready=R_SUCCEEDED(rc);record("NCM_INIT result=%08x\n",rc);
 if(ncm_ready){rc=ncmOpenContentMetaDatabase(&database,NcmStorageId_BuiltInSystem);db_ready=R_SUCCEEDED(rc);record("NCM_DATABASE result=%08x\n",rc);rc=ncmOpenContentStorage(&content_storage,NcmStorageId_BuiltInSystem);cs_ready=R_SUCCEEDED(rc);record("NCM_STORAGE result=%08x\n",rc);}
 rc=fsldrInitialize();ldr_ready=R_SUCCEEDED(rc);record("LOADER_INIT result=%08x\n",rc);checkpoint();
 for(unsigned i=0;i<sizeof(targets)/sizeof(*targets)&&!cancelled;i++)inspect_target(i,export_files);
 if(ldr_ready){fsldrExit();ldr_ready=0;}if(cs_ready){ncmContentStorageClose(&content_storage);cs_ready=0;}if(db_ready){ncmContentMetaDatabaseClose(&database);db_ready=0;}if(ncm_ready)ncmExit();
 record("\nFINISHED cancelled=%d errors=%d complete_files=%u complete_bytes=%" PRIu64 " attempted_bytes=%" PRIu64 "\n",cancelled,errors,files_complete,complete_bytes,attempted_bytes);checkpoint();
}
int main(void){
 consoleInit(NULL);Result hrc=hidInitialize();if(R_FAILED(hrc)){printf("HID failed %08x\n",hrc);consoleUpdate(NULL);svcSleepThread(3000000000LL);consoleExit(NULL);return 1;}
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
 Result rc=fsOpenSdCardFileSystem(&sd);sd_ready=R_SUCCEEDED(rc);int dirty=1,armed=0;
 while(!exit_requested){padUpdate(&pad);u64 down=padGetButtonsDown(&pad);if(!armed){if(!padGetButtons(&pad))armed=1;down=0;}
  if(down&HidNpadButton_Plus)exit_requested=1;
  if(sd_ready&&(down&(HidNpadButton_A|HidNpadButton_X))){run_probe((down&HidNpadButton_X)!=0);dirty=1;armed=0;}
  if(dirty){printf("\x1b[2J\x1b[HBROWSER RESEARCH v0.1\n\nRead-only access to six installed browser components.\nWrites only fresh output under /switch/DiscordNX2-Browser/.\nDoes not unlock, launch or patch the browser.\n\nA: inventory/access check\nX: export code and resources\nPLUS: exit\n\nSD open: %08x\nCompleted files: %u   Errors: %d\nLast run: SD:%s\n\nA .part file is incomplete; see report.txt.\n",rc,files_complete,errors,run_path[0]?run_path:"(none)");consoleUpdate(NULL);dirty=0;}
  svcSleepThread(16000000LL);
 }
 if(sd_ready)fsFsClose(&sd);
 hidExit();consoleExit(NULL);return 0;
}
