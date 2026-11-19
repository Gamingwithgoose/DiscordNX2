# SPDX-License-Identifier: MIT
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'app/source/native_login.c').read_text()
a=s.index('static Mutex native_network_mutex;');b=s.index('#define socketInitialize',a)
print('''#include <assert.h>
#include <stdio.h>
#include <stddef.h>
typedef int Mutex;typedef int Result;
#define R_SUCCEEDED(x) ((x)==0)
static int socket_inits,socket_exits,ssl_inits,ssl_exits,fail_socket,fail_ssl;
static void mutexLock(Mutex *p){(void)p;}static void mutexUnlock(Mutex *p){(void)p;}
static int socketInitialize(void *p){(void)p;socket_inits++;return fail_socket;}static void socketExit(void){socket_exits++;}
static int sslInitialize(int n){(void)n;ssl_inits++;return fail_ssl;}static void sslExit(void){ssl_exits++;}
'''+s[a:b]+'''
int main(void){
assert(native_socket_acquire()==0&&native_ssl_acquire()==0);/* voice owner */
assert(native_socket_acquire()==0&&native_ssl_acquire()==0);/* chat owner */
assert(socket_inits==1&&ssl_inits==1);native_ssl_release();native_socket_release();assert(!ssl_exits&&!socket_exits);
assert(native_socket_acquire()==0&&native_ssl_acquire()==0);/* next channel request */
native_ssl_release();native_socket_release();assert(!ssl_exits&&!socket_exits);native_ssl_release();native_socket_release();assert(ssl_exits==1&&socket_exits==1);
fail_socket=7;assert(native_socket_acquire()==7&&!native_socket_refs);fail_ssl=8;assert(native_ssl_acquire()==8&&!native_ssl_refs);native_ssl_release();native_socket_release();assert(ssl_exits==1&&socket_exits==1);
puts("PASS shared services: chat completion/navigation cannot close active call sockets/TLS; final owner cleanup and failed acquire");}
''')
