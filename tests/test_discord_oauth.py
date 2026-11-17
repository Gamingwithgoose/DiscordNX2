import base64, hashlib, unittest, urllib.parse, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import discord_login as o
class TestOAuth(unittest.TestCase):
    def test_pkce(self):
        v,c=o.pkce_pair()
        self.assertGreaterEqual(len(v),43)
        self.assertEqual(c,base64.urlsafe_b64encode(hashlib.sha256(v.encode()).digest()).decode().rstrip("="))
    def test_authorize(self):
        url=o.authorize_url("123456789012345678","state","challenge")
        q=urllib.parse.parse_qs(urllib.parse.urlsplit(url).query)
        self.assertEqual(q["scope"],["identify guilds"])
        self.assertEqual(q["code_challenge_method"],["S256"])
        self.assertEqual(q["redirect_uri"],[o.REDIRECT_URI])
        self.assertNotIn("client_secret",q)
    def test_localhost(self):
        self.assertEqual(o.HOST,"127.0.0.1")
        self.assertEqual(o.PORT,8765)
if __name__=="__main__":unittest.main()
