"""Actual MainWindow -> complete ordinary CLI -> existing HTTP journal/native adapter.
Only engine and GPU inventory are synthetic. No production requests or GPU kernels.
"""
import contextlib,hashlib,importlib.util,io,json,os,subprocess,sys,time,unittest,uuid
from unittest.mock import patch
from pathlib import Path
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve();GUI=Path(sys.argv.pop(1)).resolve();PROOF=Path(sys.argv.pop(1)).resolve()
saved=sys.argv[:];sys.argv=['fixture',str(CLI),str(ADAPTER)]
spec=importlib.util.spec_from_file_location('OrdinaryFixture',Path(__file__).with_name('CommonRouteTests.py'));fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture);sys.argv=saved
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'measured'))
from CommonAcceptance import CommonAcceptanceServer,private_json
class Tests(unittest.TestCase):
 def setUp(self):
  self.f=fixture.Tests();self.f.setUp();self.f.server.shutdown();self.f.server.server_close();self.f.thread.join();self.peer=None
  self.root=self.f.root/'peer';self.root.mkdir();self.journal=self.root/'http-live.json';self.identity=dict(client=self.f.client,assignment=self.f.assignment,scope=self.f.count,profile=hashlib.sha256(self.f.profile.read_bytes()).hexdigest())
 def tearDown(self):
  try:
   subprocess.run([str(CLI),'stop'],env=self.f.env,capture_output=True,timeout=10)
   deadline=time.monotonic()+5
   while (self.f.home/'.local/share/OpenPuzzle/runtime.pid').exists() and time.monotonic()<deadline:time.sleep(.02)
  finally:
   if self.peer:self.peer.close()
   self.f.tearDown()
 def server(self,port=0):
  self.peer=CommonAcceptanceServer(self.root,cli=CLI,adapter=ADAPTER,engine=self.f.engine,count=self.f.count,grid=128,target=self.f.target,planning=2500,gpu_uuid=self.f.uuid,client_id=self.f.client,assignment_id=self.f.assignment,port=port,journal=self.journal,identity=self.identity)
  self.f.env.update(OPENPUZZLE_SERVER_URL=self.peer.url,OPENPUZZLE_CLI=str(CLI),OPV_ACCEPTANCE_WORKSPACE=str(self.f.workspace),QT_QPA_PLATFORM='offscreen')
 def gui(self,phase):
  result=subprocess.run([str(GUI),phase],env=self.f.env,capture_output=True,text=True,timeout=45)
  log=self.f.home/'.local/share/OpenPuzzle/ui-runtime.log';details=log.read_text()[-5000:] if log.exists() else ''
  self.assertEqual(result.returncode,0,result.stdout+result.stderr+details)
 def test_real_main_window_stop_close_restart_peer_reopen_resume_safe_stop(self):
  self.server();port=self.peer.server.server_port;self.gui('pause')
  self.assertEqual(self.peer.review()['claims'],1);self.assertEqual(self.peer.review()['completions'],0);self.assertTrue(self.f.state.exists())
  before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in self.f.workspace.glob('vanity-native/attempt-*/engine.log')}
  self.assertTrue(before);self.peer.close();self.peer=None;self.server(port);self.gui('resume-safe')
  self.assertEqual(self.peer.review()['claim_requests'],1);self.assertEqual(self.peer.review()['claims'],1);self.assertEqual(self.peer.review()['completions'],1);self.assertEqual(self.peer.errors,[])
  for name,sha in before.items():self.assertEqual(hashlib.sha256(Path(name).read_bytes()).hexdigest(),sha)
  result=subprocess.run([str(PROOF),'inspect',str(self.f.workspace),str(self.f.count)],env=self.f.env,capture_output=True,text=True,timeout=10)
  self.assertEqual(result.returncode,0,result.stderr);proof=json.loads(result.stdout);self.assertTrue(proof['proof']);self.assertEqual(int(proof['checked']),self.f.count)
  self.assertFalse(self.f.state.exists());self.assertEqual(len(list(self.f.workspace.glob('vanity-native/attempt-*'))),2)
 def test_peer_uses_existing_idempotent_completion_contract(self):
  self.server();peer=self.peer
  claim=dict(client_id=self.f.client,puzzle=69,execution_slot='primary',backend='cuda',target_duration_minutes=60,speed_mkeys=2500,requested_keys=2**38)
  self.assertEqual(peer.handle('/api/range/claim',claim)[0],200);self.assertEqual(peer.handle('/api/range/claim',claim)[1]['assignment_id'],self.f.assignment)
  completion=dict(client_id=self.f.client,assignment_id=self.f.assignment,status='completed',exit_code=0,keys_checked=str(self.f.count))
  self.assertFalse(peer.handle('/api/range/complete',completion)[1]['already_finalized']);self.assertTrue(peer.handle('/api/range/complete',completion)[1]['already_finalized']);self.assertEqual(peer.completions,1)
  self.assertEqual(peer.handle('/api/range/claim',claim)[0],404)
 def test_preview_default_only_verifies_does_not_open_ui_or_gpu(self):
  saved=sys.argv[:];sys.argv=['measurement',str(CLI)]
  spec=importlib.util.spec_from_file_location('PreviewMeasurement',Path(__file__).with_name('MeasuredProfileTests.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);sys.argv=saved
  measured=module.Tests();measured.setUp()
  try:
   measured.install();import CommonProfile,CommonPreview
   report=measured.root/'prepared';report.mkdir();source=report/'source';source.mkdir();kit=report/'kit';kit.mkdir()
   code=source/'marker';code.write_text('Bound source');manifest={'marker':hashlib.sha256(code.read_bytes()).hexdigest()};(kit/'SOURCE.json').write_text(json.dumps(manifest))
   bound=CommonProfile.bind(measured.store,CLI,ADAPTER,report/'run-profile')
   summary=dict(version='0.20.1',status='prepared_common_route_validated',scope_suites=14,production_enabled=False,source=str(source),source_manifest_sha256=hashlib.sha256((kit/'SOURCE.json').read_bytes()).hexdigest(),profile_store=str(measured.store),run_profile=bound)
   for name,path in [('cli',CLI),('ui',GUI),('adapter',ADAPTER)]:summary[name]=str(path);summary[name+'_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
   (report/'SUMMARY.json').write_text(json.dumps(summary))
   with patch.object(sys,'argv',['preview']),patch.object(CommonPreview.subprocess,'run',side_effect=AssertionError('Must not launch any process')),contextlib.redirect_stdout(io.StringIO()):
    self.assertEqual(CommonPreview.main(report),0)
   self.assertFalse((report/'acceptance').exists())
   ui_calls=[]
   def launch(command,**kwargs):
    if command[0]=='nvidia-smi':return subprocess.CompletedProcess(command,0,stdout='3, '+measured.record['uuid']+'\n',stderr='')
    self.assertEqual(command,[str(GUI)]);env=kwargs['env'];self.assertEqual(env['OPENPUZZLE_CLI'],str(CLI));self.assertTrue(env['OPENPUZZLE_SERVER_URL'].startswith('http://127.0.0.1:'))
    options=CommonPreview.configparser.ConfigParser();options.read(Path(env['HOME'])/'.config/OpenPuzzle Project/OpenPuzzle.conf')
    self.assertEqual(options['run']['mode'],'5');self.assertEqual(options['run']['puzzle'],'69');self.assertEqual(options['run']['device'],'3');ui_calls.append(command)
    return subprocess.CompletedProcess(command,0)
   for _ in range(2):
    with patch.object(sys,'argv',['preview','--run']),patch.object(CommonPreview.subprocess,'run',side_effect=launch),contextlib.redirect_stdout(io.StringIO()):self.assertEqual(CommonPreview.main(report),0)
   self.assertEqual(len(ui_calls),2);self.assertEqual(len(list((report/'acceptance').glob('run-*'))),1)
   summary['ui_sha256']='0'*64;(report/'SUMMARY.json').write_text(json.dumps(summary))
   with patch.object(sys,'argv',['preview']),self.assertRaisesRegex(RuntimeError,'Prepared binary changed'):CommonPreview.main(report)
  finally:measured.tearDown()
 def test_pending_or_changed_journal_is_preserved_and_blocks_reopen(self):
  self.server();port=self.peer.server.server_port;self.peer.close();self.peer=None
  original=self.journal.read_bytes();value=json.loads(original);value['pending']=dict(path='/api/range/claim');self.journal.write_text(json.dumps(value))
  before=self.journal.read_bytes()
  with self.assertRaisesRegex(RuntimeError,'Incomplete/different'):self.server(port)
  self.assertEqual(self.journal.read_bytes(),before)
if __name__=='__main__':unittest.main(verbosity=2)
