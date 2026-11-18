// SPDX-License-Identifier: MIT
#ifndef DNX2_NATIVE_LOGIN_H
#define DNX2_NATIVE_LOGIN_H
#define DNX2_MAX_SERVERS 64
typedef struct {char id[24];char name[80];} Dnx2Server;
typedef struct {
    int signed_in,server_count,selected;
    char username[64],display_name[80];
    Dnx2Server servers[DNX2_MAX_SERVERS];
} Dnx2Account;
typedef void (*Dnx2AuthStatus)(const char *stage,const char *qr_url);
typedef int (*Dnx2AuthCancel)(void);
const Dnx2Account *dnx2Account(void);
void dnx2AccountSelect(int index);
void dnx2AccountLogout(void);
/* Returns 1 when account and actual server data fetched; 0 otherwise.
 * No account token, RSA key, ticket, nonce, or QR fingerprint ever logged. */
int dnx2NativeQrLogin(Dnx2AuthStatus status,Dnx2AuthCancel cancel);
#endif
