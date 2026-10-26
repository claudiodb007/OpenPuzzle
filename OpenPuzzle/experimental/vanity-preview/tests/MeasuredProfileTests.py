#!/usr/bin/env python3
"""Synthetic profile and native CLI/HTTP tests. Never measures a GPU."""
import importlib.util,json,os,signal,subprocess,sys,tempfile,threading,time,unittest
from pathlib import Path
from unittest.mock import patch
CLI=Path(sys.argv.pop(1)).resolve()
ROOT=Path(__file__).resolve().parent.parent/'measured';sys.path.insert(0,str(ROOT))
import Profile as p
import Evidence as e
import Support as s
UUID='GPU-11111111-2222-3333-4444-555555555555'
class Tests(unittest.TestCase):
 def setUp(self):
  os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix="opv measured ç ' $() ");self.root=Path(self.tmp.name)
  self.store=self.root/'profiles';self.store.mkdir();self.evidence=self.root/'MEASUREMENT.json'
  self.engine=Path(__file__).with_name('FakeTargetEngine.py').resolve();self.adapter=CLI.parent/'openpuzzle-vanity-adapter'
  self.paths=dict(cli=CLI,adapter=self.adapter,engine=self.engine,bitcrack=CLI)
  self.prior=self.root/'prior';self.prior.mkdir();s.write_json(self.prior/'SUMMARY.json',dict(version='0.6.0',status='completed',real_gpu_requested=True,production_enabled=False))
  source=self.root/'HTTP_SOURCE.json';source.write_bytes((ROOT/'HTTP_SOURCE.json').read_bytes())
  rows=[]
  for kind in ('bitcrack','vanity-cli'):
   for pair in range(3):
    rate=(1500 if kind=='bitcrack' else 2600)+pair;row=dict(label=f'measure-{pair}-{kind}',engine=kind,pair=pair,status='passed',keys=e.MEASURE_COUNT,wall_seconds=e.MEASURE_COUNT/rate/1e6,wall_mkeys_per_second=rate);rows.append(row)
    d=self.root/row['label'];d.mkdir();s.write_json(d/'result.json',row)
  self.proof=dict(version='0.7.0',status='completed',production_enabled=False,count=e.MEASURE_COUNT,grid=1024,threads=256,points=1024,gpu='NVIDIA GeForce RTX 4070 SUPER',uuid=UUID,driver='synthetic-driver',power_limit_w=245,measurements=rows,executables={k:dict(path=str(v),sha256=s.sha(v)) for k,v in self.paths.items()},prior_report=str(self.prior),prior_summary_sha256=s.sha(self.prior/'SUMMARY.json'),source_checksums=dict(path=str(source),sha256=s.sha(source)))
  s.write_json(self.evidence,self.proof);self.record=e.profile(self.proof,self.evidence,self.paths);self.candidate=self.root/'candidate.json';s.write_json(self.candidate,self.record)
 def tearDown(self):self.tmp.cleanup()
 def cli(self,*args,code=0):
  r=subprocess.run([str(CLI),'vanity-preview',*map(str,args)],text=True,capture_output=True,timeout=30)
  self.assertEqual(r.returncode,code,r.stdout+r.stderr);return r.stdout
 def install(self):return p.import_profile(self.candidate,self.store)
 def test_native_cli_import_show_reopen_and_idempotent(self):
  prior=self.root/'bitcrack';prior.write_text('preserve')
  args=['measured-import','--from',self.candidate,'--store',self.store,'--enable-experimental-vanity-local']
  record=json.loads(self.cli(*args));self.assertAlmostEqual(record['planning_mkeys'],2522.97)
  before=(self.store/(self.record['binding_sha256']+'.json')).read_bytes();self.cli(*args)
  self.assertEqual(before,(self.store/(self.record['binding_sha256']+'.json')).read_bytes());self.assertEqual(prior.read_text(),'preserve')
  self.assertEqual(json.loads(self.cli('measured-show','--store',self.store))['status'],'ready');self.assertEqual((self.store/'selected.json').stat().st_mode&0o777,0o600)
 def test_tampered_rate_evidence_binary_and_unknown_fields(self):
  original=self.candidate.read_bytes()
  for key,value in [('planning_mkeys',2601),('backend','opencl'),('production_enabled',True),('extra',True)]:
   bad=dict(self.record);bad[key]=value;s.write_json(self.candidate,bad)
   with self.assertRaises(Exception):self.install()
  self.candidate.write_bytes(original);self.proof['measurements'][0]['wall_seconds']=1;s.write_json(self.evidence,self.proof)
  with self.assertRaises(Exception):self.install()
  self.assertFalse((self.store/'selected.json').exists())
 def test_changed_per_run_and_source_rejected(self):
  row=self.proof['measurements'][0];path=self.root/row['label']/'result.json';path.write_text('{}')
  with self.assertRaises(Exception):self.install()
  s.write_json(path,row);(self.root/'HTTP_SOURCE.json').write_text('{}')
  with self.assertRaises(Exception):self.install()
 def test_unsafe_existing_paths_preserved(self):
  self.store.chmod(0o755)
  with self.assertRaises(Exception):self.install()
  self.store.chmod(0o700);(self.store/'selected.json').symlink_to(self.candidate)
  before=self.candidate.read_bytes()
  with self.assertRaises(Exception):self.install()
  self.assertEqual(before,self.candidate.read_bytes())
 def test_corrupt_selection_or_partial_publication_preserved(self):
  self.install();(self.store/'selected.json').write_text('broken')
  with self.assertRaises(Exception):self.install()
  self.assertEqual((self.store/'selected.json').read_text(),'broken')
 def test_cli_missing_optin_and_server_rejected(self):
  self.cli('measured-import','--from',self.candidate,'--store',self.store,code=2)
  self.cli('measured-show','--store',self.store,'--server','https://example.invalid',code=2)
  self.assertFalse((self.store/'selected.json').exists())
 def test_gpu_driver_limit_or_busy_refused(self):
  for name,driver,limit in [('wrong','synthetic-driver',245),(self.record['gpu'],'changed',245),(self.record['gpu'],'synthetic-driver',300)]:
   with patch.object(s,'smi',return_value=[[UUID,name,driver]]),patch.object(s,'require_idle'),patch.object(s,'telemetry',return_value={'power_limit_w':limit}):
    with self.assertRaises(Exception):p.hardware(self.record)
 def local(self,kind):
  self.install();directory=self.root/'run';old=p.m.LocalMeasurementServer
  # Set a marker as soon as a native nonzero progress arrives; GUI follows this same control path.
  def server(count,address):
   result=old(count,address)
   if kind!='normal':
    def trigger():
     until=time.monotonic()+20
     while time.monotonic()<until:
      if result.progress and result.progress[-1]>0:
       (directory/'controls'/('safe-stop' if kind=='safe' else 'cancel')).touch();return
      time.sleep(.005)
    thread=threading.Thread(target=trigger,daemon=True);thread.start()
   return result
  with patch.object(p,'hardware',return_value=(UUID,245)),patch.object(s,'telemetry',return_value=dict(temperature_c=30,power_limit_w=245)),patch.object(s,'compute_pids',return_value=[]),patch.object(p.m,'LocalMeasurementServer',side_effect=server):
   if kind=='cancel':
    self.assertEqual(p.local_run(self.store,directory),130)
   else:self.assertEqual(p.local_run(self.store,directory),0)
  return directory
 def test_native_measured_assignment_and_safe_stop(self):
  for kind in ('normal','safe'):
   if kind=='safe':self.root=self.root/'safe';self.root.mkdir();self.store=self.root/'profiles';self.store.mkdir()
   directory=self.local(kind);summary=e.read_json(directory/'SUMMARY.json');self.assertEqual(summary['status'],'completed');self.assertFalse(summary['production_enabled'])
   calls=e.read_json(directory/'http-requests.json');self.assertEqual(sum(r['path'].endswith('/claim') for r in calls),1);self.assertEqual(sum(r['path'].endswith('/complete') for r in calls),1)
 def test_native_cancel_preserves_checkpoint(self):
  d=self.local('cancel');self.assertTrue((d/'state/assignment/checkpoint.txt').is_file());self.assertEqual(e.read_json(d/'SUMMARY.json')['status'],'cancelled')
 def test_owned_cli_signal_cancels_actual_adapter(self):
  self.install();tools=self.root/'tools';tools.mkdir();smi=tools/'nvidia-smi'
  smi.write_text('#!/usr/bin/env python3\nimport sys\n'+
   'query=sys.argv[1]\n'+
   'if query.startswith("--query-compute-apps"):pass\n'+
   'elif query=="--query-gpu=uuid,name,driver_version":print("'+UUID+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
   'else:print("'+UUID+',30,20,245,0,1000,1000,50")\n');smi.chmod(0o700)
  d=self.store/'run-11111111-2222-3333-4444-555555555555';env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'])
  with (self.root/'signal.log').open('w') as log:
   child=subprocess.Popen([str(CLI),'vanity-preview','measured-test','--store',str(self.store),'--run-root',str(d),'--enable-experimental-vanity-local'],stdout=log,stderr=log,env=env,start_new_session=True)
   try:
    deadline=time.monotonic()+15;cp=d/'state/assignment/checkpoint.txt'
    while time.monotonic()<deadline:
     if cp.is_file() and int(cp.read_text().split()[6])>0:break
     self.assertIsNone(child.poll());time.sleep(.02)
    self.assertTrue(cp.is_file());child.terminate();self.assertEqual(child.wait(timeout=15),130)
    self.assertEqual(e.read_json(d/'SUMMARY.json')['status'],'cancelled');self.assertEqual(cp.read_text().split()[7],'interrupted')
   finally:s.stop_owned(child)
 def test_repeat_success_selects_only_verified_candidate(self):
  self.install();before=(self.store/'selected.json').read_bytes()
  import types
  module=types.SimpleNamespace(main=lambda:self.root)
  # Measurement returns the conventional candidate filename.
  (self.root/'vanity-cli-measured.profile.json').write_bytes(self.candidate.read_bytes())
  with patch.object(p,'hardware'),patch.object(p.importlib.util,'module_from_spec',return_value=module),patch('importlib.machinery.SourceFileLoader.exec_module'):
   p.benchmark(self.store)
  self.assertEqual(before,(self.store/'selected.json').read_bytes())
  self.assertEqual(p.read_selected(self.store)[0],self.record)
 def test_cancelled_or_unstable_repeat_preserves_selection(self):
  self.install();before=(self.store/'selected.json').read_bytes()
  import types
  for failure in (RuntimeError('unstable'),KeyboardInterrupt()):
   def fail():raise failure
   module=types.SimpleNamespace(main=fail)
   with patch.object(p,'hardware'),patch.object(p.importlib.util,'module_from_spec',return_value=module),patch('importlib.machinery.SourceFileLoader.exec_module'):
    with self.assertRaises(type(failure)):p.benchmark(self.store)
   self.assertEqual(before,(self.store/'selected.json').read_bytes())
 def test_failed_repeat_preserves_selection(self):
  self.install();before=(self.store/'selected.json').read_bytes()
  with patch.object(p,'hardware',side_effect=RuntimeError('busy')):
   with self.assertRaises(RuntimeError):p.benchmark(self.store)
  self.assertEqual(before,(self.store/'selected.json').read_bytes())
if __name__=='__main__':unittest.main(verbosity=2)
