#!/usr/bin/env python3
"""Tests the real OpenPuzzle CLI route with a fake CUDA child, no hardware/network."""
import hashlib,importlib.util,json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
SUPPORT=Path(__file__).parent/'support';sys.path.insert(0,str(SUPPORT))
import LocalScenarios as m
import Support as s
UUID='GPU-11111111-2222-3333-4444-555555555555'
def profile_text(engine,adapter,evidence):
    q=lambda value:json.dumps(str(value),ensure_ascii=False)
    return '\n'.join(['OPV-CLI-PROFILE-1',
      'engine '+q(engine),'engine_sha256 '+q(s.sha(engine)),
      'adapter '+q(adapter),'adapter_sha256 '+q(s.sha(adapter)),
      'gpu_uuid '+q(UUID),'gpu_name "NVIDIA GeForce RTX 4070 SUPER"',
      'grid 1024','threads 256','points 1024','median_mkeys 2600','minimum_mkeys 2599','maximum_mkeys 2601','planning_mkeys 2522','samples 3',
      'performance_summary '+q(evidence),'performance_sha256 '+q(s.sha(evidence)),
      'measurement_adapter_sha256 '+q(s.sha(adapter)),
      'integration_summary '+q(evidence),'integration_sha256 '+q(s.sha(evidence))])+'\n'
from CliScenarioDriver import CliScenarios
class CliTests(unittest.TestCase):
    def setUp(self):
        os.umask(0o077);self.temp=tempfile.TemporaryDirectory(prefix="OpenPuzzle CLI ç ; $() '");self.root=Path(self.temp.name)
        self.engine=self.root/'fake engine';source=Path(__file__).with_name('FakeEngine.py').read_text().replace('from BuildSupport import key_hash','from Support import key_hash');self.engine.write_text(source);self.engine.chmod(0o700)
        self.evidence=self.root/'evidence.json';self.evidence.write_text('{}\n');self.manifest=self.root/'import.txt';self.manifest.write_text(profile_text(self.engine,ADAPTER,self.evidence))
        self.profile=self.root/'vanity.profile';self.env=os.environ.copy();self.env.update(HOME=str(self.root/'home'),XDG_CONFIG_HOME=str(self.root/'home/.config'),XDG_DATA_HOME=str(self.root/'home/.local/share'),OPV_FAKE_MODE='driver',OPV_TEST_LIBRARY=str(SUPPORT),CUDA_VISIBLE_DEVICES=UUID)
        (self.root/'home').mkdir()
    def tearDown(self):self.temp.cleanup()
    def command(self,*args,code=0):
        r=subprocess.run([str(CLI),'vanity-preview',*[str(a) for a in args]],env=self.env,capture_output=True,text=True,timeout=30)
        self.assertEqual(r.returncode,code,r.stdout+r.stderr);return r.stdout+r.stderr
    def import_profile(self):self.command('profile-import','--from',self.manifest,'--profile',self.profile,'--enable-experimental-vanity-local')
    def test_import_show_verify_idempotency_and_legacy_preservation(self):
        legacy=self.root/'bitcrack.profile';legacy.write_text('existing profile\n');self.import_profile();before=self.profile.read_bytes()
        self.import_profile();self.assertEqual(before,self.profile.read_bytes());self.assertEqual(legacy.read_text(),'existing profile\n')
        output=self.command('profile-show','--profile',self.profile);self.assertIn('Planning MKey/s (3% margin): 2522',output);self.assertIn('Production assignments: disabled',output)
        self.command('verify','--profile',self.profile);self.assertEqual(self.profile.stat().st_mode&0o777,0o600)
        self.assertEqual(list((self.root/'home').iterdir()),[])
    def test_invalid_or_missing_fields_and_optin_refused(self):
        self.command('profile-import','--from',self.manifest,'--profile',self.profile,code=2);self.assertFalse(self.profile.exists())
        original=self.manifest.read_text()
        for data in [original.replace('threads 256','threads 128'),original.replace('planning_mkeys 2522','planning_mkeys 2600'),original.replace('samples 3','samples 0'),original+'extra field\n',original.replace('median_mkeys 2600','median_mkeys nan')]:
            self.manifest.write_text(data);self.command('profile-import','--from',self.manifest,'--profile',self.profile,'--enable-experimental-vanity-local',code=2);self.assertFalse(self.profile.exists())
    def test_changed_files_or_profile_are_preserved(self):
        self.import_profile();before=self.profile.read_bytes();self.evidence.write_text('changed\n')
        self.command('verify','--profile',self.profile,code=2);self.assertEqual(before,self.profile.read_bytes());self.evidence.write_text('{}\n')
        self.profile.write_text('damaged profile\n');self.command('profile-import','--from',self.manifest,'--profile',self.profile,'--enable-experimental-vanity-local',code=2);self.assertEqual(self.profile.read_text(),'damaged profile\n')
    def test_symlink_and_public_directory_refused(self):
        self.profile.symlink_to(self.manifest);before=self.manifest.read_bytes()
        self.command('profile-import','--from',self.manifest,'--profile',self.profile,'--enable-experimental-vanity-local',code=2);self.assertEqual(before,self.manifest.read_bytes());self.profile.unlink()
        self.root.chmod(0o755);self.command('profile-import','--from',self.manifest,'--profile',self.profile,'--enable-experimental-vanity-local',code=2);self.assertFalse(self.profile.exists());self.root.chmod(0o700)
    def test_local_scenarios_through_real_cli(self):
        self.import_profile();runner=CliScenarios(self.root,ADAPTER,CLI,self.engine,self.env,profile=self.profile)
        rows=runner.run_all(False);self.assertEqual(len(rows),17)
    def test_wrong_gpu_and_server_option_block_before_claim(self):
        self.import_profile();runner=CliScenarios(self.root,ADAPTER,CLI,self.engine,self.env,profile=self.profile);d,args=runner.setup('normal',786439,1)
        self.env['CUDA_VISIBLE_DEVICES']='0'
        r=subprocess.run([str(x) for x in args],env=self.env,capture_output=True,text=True);self.assertEqual(r.returncode,2);self.assertFalse((d/'state').exists())
        self.command('verify','--profile',self.profile,'--server','https://example.invalid',code=2)
    def test_help_is_local(self):
        self.assertIn('No production assignments',self.command('help'));self.assertEqual(list((self.root/'home').iterdir()),[])
if __name__=='__main__':unittest.main(verbosity=2)
