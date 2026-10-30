"""Actual cancellation sync and HTTP retries, stopped fixture identity, no GPU."""
import json,os,subprocess,sys,tempfile,threading,unittest
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
DRIVER=Path(sys.argv.pop(1)).resolve()
class Tests(unittest.TestCase):
 def setUp(self):
  os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix='opv-cancel-sync-');self.root=Path(self.tmp.name)
  self.calls=[];self.lost=False;self.temporary=False;self.effects=set()
  owner=self
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_POST(self):
    q=json.loads(self.rfile.read(int(self.headers['Content-Length'])));owner.calls.append((self.path,q))
    owner.assertEqual(self.path,'/api/range/complete');owner.assertEqual(q['status'],'cancelled');owner.assertEqual(q['exit_code'],-2)
    if owner.temporary:code=503;body=dict(error='temporary_unavailable')
    else:
     owner.effects.add(q['assignment_id']);code=200;body=dict(success=True,status='cancelled',already_finalized=len(owner.calls)>1)
     if owner.lost:owner.lost=False;self.close_connection=True;return
    data=json.dumps(body).encode();self.send_response(code);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
  self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.daemon_threads=True;self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
  self.url=f'http://127.0.0.1:{self.server.server_port}'
  self.env={k:v for k,v in os.environ.items() if not k.startswith(('OPENPUZZLE_','GIT_'))};self.env.update(HOME=str(self.root),XDG_DATA_HOME=str(self.root/'.local/share'),XDG_CONFIG_HOME=str(self.root/'.config'))
 def tearDown(self):self.server.shutdown();self.server.server_close();self.thread.join();self.tmp.cleanup()
 def execute(self,action):
  r=subprocess.run([str(DRIVER),action,self.url,str(self.root/'workspace')],env=self.env,capture_output=True,text=True,timeout=20)
  self.assertEqual(r.returncode,0,r.stdout+r.stderr);return json.loads(r.stdout)
 def test_cancel_upload_and_cleanup(self):
  r=self.execute('request');self.assertEqual(r,dict(uploaded=1,removed=1,preserved=0,exit_code=-2));self.assertEqual(len(self.calls),1)
 def test_temporary_failure_preserves_intent_and_retries_only_cancel(self):
  self.temporary=True;r=self.execute('request');self.assertFalse(r['removed']);self.assertTrue((self.root/'workspace/cancel-requested.txt').exists())
  self.temporary=False;r=self.execute('retry');self.assertTrue(r['removed']);self.assertEqual(len(self.effects),1);self.assertEqual(len(self.calls),2)
 def test_lost_response_retries_same_cancel_and_one_effect(self):
  self.lost=True;r=self.execute('request');self.assertFalse(r['removed']);r=self.execute('retry');self.assertTrue(r['removed']);self.assertEqual(len(self.effects),1);self.assertEqual(len(self.calls),2)
 def test_interruption_without_user_stop_preserves_recovery_and_sends_nothing(self):
  r=self.execute('recover');self.assertTrue(r['preserved']);self.assertFalse(r['removed']);self.assertEqual(self.calls,[])
if __name__=='__main__':unittest.main(verbosity=2)
