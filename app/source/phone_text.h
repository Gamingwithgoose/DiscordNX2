// SPDX-License-Identifier: MIT
#ifndef DNX2_PHONE_TEXT_H
#define DNX2_PHONE_TEXT_H
#include <ctype.h>
#include <stdint.h>
#include <string.h>
static inline int dnx_keyboard_caps(const char *text){size_t n=strlen(text);while(n&&strchr(" \t\r\n\"')",text[n-1]))n--;return !n||strchr(".!?",text[n-1])!=NULL;}
static inline void dnx_keyboard_space(char text[513],uint64_t now,uint64_t *previous){size_t n=strlen(text);if(n>=512)return;if(*previous&&now-*previous<=400&&n>=2&&text[n-1]==' '&&(isalnum((unsigned char)text[n-2])||(unsigned char)text[n-2]>=128)){text[n-1]='.';text[n]=' ';text[n+1]=0;*previous=0;return;}text[n]=' ';text[n+1]=0;*previous=now;}
static inline size_t dnx_word_start(const char *text){size_t n=strlen(text);while(n&&isalpha((unsigned char)text[n-1])&&(unsigned char)text[n-1]<128)n--;return n;}
static inline int dnx_suggestions(const char *text,char out[3][40]){
    static const char *words[]={"about","actually","after","again","all","also","always","another","any","are","around","audio","awesome","back","because","been","before","better","build","but","can","channel","chat","check","come","could","day","did","different","discord","does","done","down","even","everyone","first","fix","for","friend","from","game","games","get","getting","give","good","got","great","guess","had","has","have","hello","help","here","hey","how","image","into","just","keep","know","last","later","let","like","little","loading","login","look","lot","love","make","maybe","message","more","morning","much","need","new","next","nice","night","not","now","off","okay","one","only","other","our","out","over","people","play","please","pretty","problem","really","right","same","say","see","send","server","should","show","some","something","sorry","sounds","still","sure","switch","take","test","testing","text","than","thank","thanks","that","the","their","them","then","there","these","they","thing","think","this","those","time","today","tomorrow","too","try","update","use","using","very","voice","wait","want","was","way","well","were","what","when","where","which","who","why","will","with","work","working","would","yeah","yes","you","your"};
    size_t n=strlen(text),start=dnx_word_start(text),len=n-start;memset(out,0,120);int count=0;
    if(len){if(len>31)return 0;char prefix[40];for(size_t i=0;i<len;i++)prefix[i]=(char)tolower((unsigned char)text[start+i]);prefix[len]=0;for(size_t i=0;i<sizeof(words)/sizeof(words[0])&&count<3;i++)if(!strncmp(words[i],prefix,len)){strcpy(out[count],words[i]);if(isupper((unsigned char)text[start]))out[count][0]=(char)toupper((unsigned char)out[count][0]);count++;}return count;}
    char last[40]={0};while(n&&isspace((unsigned char)text[n-1]))n--;size_t end=n;while(n&&isalpha((unsigned char)text[n-1]))n--;if(end-n<sizeof(last)){for(size_t i=0;i<end-n;i++)last[i]=(char)tolower((unsigned char)text[n+i]);}
    const char *a=!strcmp(last,"thank")?"you":!strcmp(last,"how")?"are":!strcmp(last,"good")?"morning":!strcmp(last,"see")?"you":!strcmp(last,"want")?"to":!strcmp(last,"going")?"to":"the";
    strcpy(out[0],a);strcpy(out[1],"I");strcpy(out[2],"you");if(dnx_keyboard_caps(text))out[0][0]=(char)toupper((unsigned char)out[0][0]);return 3;
}
static inline int dnx_suggestion_pick(char text[513],const char *word){size_t start=dnx_word_start(text),n=strlen(word);if(!n||start+n+1>512)return 0;memcpy(text+start,word,n);text[start+n]=' ';text[start+n+1]=0;return 1;}
static inline int dnx_suggestion_hit(int x,int y,int count){if(y<314||y>=346)return -1;for(int i=0;i<count;i++)if(x>=372+i*280&&x<640+i*280)return i;return -1;}
#endif
