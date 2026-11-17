#!/usr/bin/env python3
"""One-time, opt-in local profile bridge for DiscordNX2 v0.19.

Discord OAuth credentials and access tokens never leave PC memory.
The Switch receives only a bounded list of readable guild names and IDs after
explicit six-digit-code confirmation on both devices. No voice access.
"""
import argparse
import http.server
import ipaddress
import json
import os
import queue
import re
import socket
import threading
import time
import uuid

from discord_login import sign_in

DISCOVERY_PORT = 39274
HTTP_PORT = 39275
MAGIC = b"DNX2_PAIR_V1:39275"
PENDING = queue.Queue(maxsize=8)

def sanitize(user, guilds):
    cleaned = []
    for g in guilds[:100]:
        if not isinstance(g, dict):
            continue
        gid = str(g.get("id", ""))
        if not re.fullmatch(r"[0-9]{15,22}", gid):
            continue
        name = str(g.get("name", ""))[:70]
        cleaned.append({"id": gid, "name": name})
    return {
        "protocol": "dnx2-pair-v1",
        "username": str(user.get("username", ""))[:60],
        "display_name": str(user.get("global_name") or user.get("username") or "")[:60],
        "guild_count": len(cleaned),
        "guilds": cleaned,
    }

class PairHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        from urllib.parse import urlsplit, parse_qs
        route = urlsplit(self.path)
        if route.path != "/pair":
            self.send_error(404)
            return
        code = parse_qs(route.query).get("code", [""])[0]
        if not re.fullmatch(r"[0-9]{6}", code):
            self.send_error(400)
            return
        event = threading.Event()
        request = {"code": code, "remote": self.client_address[0], "event": event, "approved": False}
        try:
            PENDING.put(request, timeout=2)
        except queue.Full:
            self.send_error(429)
            return
        if not event.wait(timeout=45):
            self.send_error(408)
            return
        if not request["approved"]:
            self.send_error(403)
            return
        body = json.dumps(self.server.profile, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
        if len(body) > 64000:
            self.send_error(413)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        self.server.delivered.set()

    def log_message(self, *args):
        pass

def advertise(stop):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    try:
        while not stop.is_set():
            try:
                s.sendto(MAGIC, ("255.255.255.255", DISCOVERY_PORT))
            except OSError:
                pass
            stop.wait(1)
    finally:
        s.close()

def pair_server(profile, listen=HTTP_PORT):
    server = http.server.ThreadingHTTPServer(("0.0.0.0", listen), PairHandler)
    server.profile = profile
    server.delivered = threading.Event()
    stop = threading.Event()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    announcer = threading.Thread(target=advertise, args=(stop,), daemon=True)
    thread.start()
    announcer.start()
    print("\nOpen the DiscordNX2 Servers page on Switch 2 and choose PAIR WITH PC.")
    print("The Switch must be on the same trusted LAN as this PC.")
    print("A Windows Firewall permission prompt may appear. Allow PRIVATE networks only.")
    print("A pairing code will appear on the Switch. Confirm it here before sharing profile metadata.")
    print("No Discord token, password, email, or authorization code is sent to the Switch.")
    try:
        while not server.delivered.is_set():
            try:
                request = PENDING.get(timeout=0.25)
            except queue.Empty:
                continue
            print(f"\nSwitch pairing attempt from {request['remote']} - code {request['code']}")
            try:
                answer = input("Does this exactly match the six digits on your Switch? Type YES to approve: ").strip()
            except EOFError:
                answer = ""
            request["approved"] = (answer == "YES")
            request["event"].set()
            if request["approved"]:
                server.delivered.wait(timeout=5)
                if server.delivered.is_set():
                    print("\nProfile delivered to Switch 2. Bridge closing.")
                    break
                print("The Switch did not acknowledge the profile. Retry pairing.")
    except KeyboardInterrupt:
        print("\nPairing canceled.")
    finally:
        stop.set()
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)
        announcer.join(timeout=3)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-id", default=os.environ.get("DISCORDNX2_CLIENT_ID", ""))
    args = parser.parse_args()
    cid = args.client_id.strip()
    if not cid:
        cid = input("DiscordNX2 shared application ID (public, from developer's app): ").strip()
    if not re.fullmatch(r"[0-9]{17,22}", cid):
        raise SystemExit("Invalid public client ID; never enter a token, client secret, or password.")
    user, guilds = sign_in(cid)
    profile = sanitize(user, guilds)
    print("Authorized Discord account:", profile["display_name"])
    print("Servers available:", profile["guild_count"])
    print("This is profile-only OAuth. Discord voice permissions are NOT granted.")
    pair_server(profile)

if __name__ == "__main__":
    main()
