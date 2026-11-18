// SPDX-License-Identifier: MIT
#ifndef DNX2_PROFILE_DATA_H
#define DNX2_PROFILE_DATA_H
#include "client_json.h"
typedef struct {char id[24],name[129];int position;unsigned color;} Dnx2Role;
typedef struct {char guild[24];int loaded,count;Dnx2Role roles[256];} Dnx2GuildRoles;
typedef struct {char nick[321],avatar[80],joined[40];char roles[32][24];int count;} Dnx2Member;
static inline void dnx_roles_parse(Dnx2GuildRoles *out,const char *json){DnxJson root=dj_root(json),e;const char *p=NULL;out->loaded=1;while(out->count<256&&dj_next(root,&p,&e)){Dnx2Role *role=&out->roles[out->count];if(!dj_get(e,"id",role->id,24))continue;dj_get(e,"name",role->name,129);role->position=dj_int(e,"position",0);role->color=(unsigned)dj_int(dj_field(e,"colors"),"primary_color",dj_int(e,"color",0))&0xffffffu;out->count++;}}
static inline void dnx_member_parse(Dnx2Member *m,const char *json){DnxJson root=dj_root(json),e;const char *p=NULL;dj_get(root,"nick",m->nick,321);dj_get(root,"avatar",m->avatar,80);dj_get(root,"joined_at",m->joined,40);while(m->count<32&&dj_next(dj_field(root,"roles"),&p,&e))if(dj_string(e,m->roles[m->count],24))m->count++;}
static inline unsigned dnx_role_color(const Dnx2GuildRoles *roles,const char ids[][24],int count){int best=-2147483647;const char *best_id="";unsigned color=0;for(int i=0;i<roles->count;i++){const Dnx2Role *r=&roles->roles[i];if(!r->color)continue;for(int j=0;j<count;j++)if(!strcmp(ids[j],r->id)&&(r->position>best||(r->position==best&&strcmp(r->id,best_id)>0))){best=r->position;best_id=r->id;color=r->color;}}return color;}
#endif
