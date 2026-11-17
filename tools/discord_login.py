#!/usr/bin/env python3
"""DiscordNX2: local Discord OAuth2 authorization-code login with PKCE."""
import argparse, base64, hashlib, http.server, json, os, re, secrets
import threading, urllib.parse, urllib.request, urllib.error, webbrowser

HOST="127.0.0.1"
PORT=8765
CALLBACK="/discordnx2/callback"
REDIRECT_URI=f"http://{HOST}:{PORT}{CALLBACK}"
AUTH="https://discord.com/oauth2/authorize"
TOKEN="https://discord.com/api/oauth2/token"
API="https://discord.com/api/v10"

def pkce_pair():
    verifier=secrets.token_urlsafe(48)
    challenge=base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).decode("ascii").rstrip("=")
    return verifier,challenge

def authorize_url(client_id,state,challenge):
    params={"response_type":"code","client_id":client_id,"redirect_uri":REDIRECT_URI,
            "scope":"identify guilds","state":state,"code_challenge":challenge,
            "code_challenge_method":"S256","prompt":"consent"}
    return AUTH+"?"+urllib.parse.urlencode(params)

def request_json(url,data=None,access_token=None):
    headers={"Accept":"application/json","User-Agent":"DiscordNX2-login/1.0"}
    body=None
    if data is not None:
        body=urllib.parse.urlencode(data).encode()
        headers["Content-Type"]="application/x-www-form-urlencoded"
    if access_token: headers["Authorization"]="Bearer "+access_token
    request=urllib.request.Request(url,data=body,headers=headers)
    try:
        with urllib.request.urlopen(request,timeout=20) as response:
            return json.load(response)
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"Discord returned HTTP {e.code} (check public-client and redirect setup)") from None

class CallbackHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed=urllib.parse.urlsplit(self.path)
        if parsed.path!=CALLBACK:
            self.send_error(404);return
        q=urllib.parse.parse_qs(parsed.query)
        state=q.get("state",[""])[0]
        if not secrets.compare_digest(state,self.server.oauth_state):
            self.reply(400,"Authorization state mismatch.");return
        issuer=q.get("iss",[""])[0]
        if issuer and issuer!="https://discord.com":
            self.reply(400,"Unexpected OAuth issuer.");return
        self.server.oauth_result={
            "code":q.get("code",[""])[0],"error":q.get("error",[""])[0]}
        self.reply(200,"DiscordNX2 authorization received. You may close this tab.")
        self.server.oauth_event.set()
    def reply(self,code,message):
        body=("<!doctype html><meta charset=utf-8><h2>"+message+"</h2>").encode()
        self.send_response(code)
        self.send_header("Content-Type","text/html; charset=utf-8")
        self.send_header("Cache-Control","no-store")
        self.send_header("Content-Security-Policy","default-src 'none'")
        self.send_header("Content-Length",str(len(body)))
        self.end_headers();self.wfile.write(body)
    def log_message(self,*args): pass  # callback includes private code and state

def sign_in(client_id,open_browser=True,timeout=180):
    verifier,challenge=pkce_pair()
    state=secrets.token_urlsafe(32)
    server=http.server.ThreadingHTTPServer((HOST,PORT),CallbackHandler)
    server.oauth_state=state
    server.oauth_event=threading.Event()
    server.oauth_result=None
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    try:
        url=authorize_url(client_id,state,challenge)
        print("\nOpen Discord's sign-in page in your PC browser:")
        print(url)
        if open_browser: webbrowser.open(url)
        print(f"\nWaiting for Discord to redirect to {REDIRECT_URI}")
        if not server.oauth_event.wait(timeout):
            raise RuntimeError("Authorization timed out")
        result=server.oauth_result or {}
        if result.get("error"):raise RuntimeError("Discord authorization was declined")
        code=result.get("code")
        if not code:raise RuntimeError("Discord sent no authorization code")
        token=request_json(TOKEN,{
            "client_id":client_id,"grant_type":"authorization_code",
            "code":code,"redirect_uri":REDIRECT_URI,
            "code_verifier":verifier})
        access=token.get("access_token")
        if not access:raise RuntimeError("Discord did not issue an access token")
        user=request_json(API+"/users/@me",access_token=access)
        guilds=request_json(API+"/users/@me/guilds?limit=200",access_token=access)
        if not isinstance(user,dict) or not isinstance(guilds,list):
            raise RuntimeError("Discord returned unexpected account data")
        return user,guilds
    finally:
        server.shutdown();server.server_close();thread.join(timeout=3)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--client-id",default=os.getenv("DISCORDNX2_CLIENT_ID",""))
    parser.add_argument("--no-browser",action="store_true")
    args=parser.parse_args()
    client_id=args.client_id.strip()
    if not client_id:client_id=input("Enter your Discord application ID (public client ID only): ").strip()
    if not re.fullmatch(r"[0-9]{17,22}",client_id):
        raise SystemExit("Invalid Discord application ID. Never enter a client secret or user token.")
    try:
        user,guilds=sign_in(client_id,not args.no_browser)
    except (RuntimeError,OSError,ValueError,urllib.error.URLError) as err:
        raise SystemExit(f"Discord sign-in failed: {err}") from None
    print("\nDISCORD ACCOUNT SIGN-IN SUCCESS")
    print("Display name:",user.get("global_name") or user.get("username","unknown"))
    print("Username:",user.get("username","unknown"))
    print("User ID:",user.get("id","unknown"))
    print("Servers visible:",len(guilds))
    for guild in sorted(guilds,key=lambda g:g.get("name","").lower())[:30]:
        print(" -",guild.get("name","Unnamed"))
    print("\nNo token, password or client secret was saved.")
    print("Account authorization only: joining voice isn't supported by identify/guilds.")

if __name__=="__main__":main()
