"""Reuses the existing synthetic measurement fixture; never touches a GPU."""
import importlib.util,os,sys,unittest
from pathlib import Path
CLI=Path(sys.argv.pop(1)).resolve()
sys.argv.insert(1,str(CLI))
spec=importlib.util.spec_from_file_location('MeasuredFixture',Path(__file__).with_name('MeasuredProfileTests.py'))
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
import CommonProfile
class Tests(unittest.TestCase):
 def setUp(self):
  self.fixture=fixture.Tests();self.fixture.setUp();self.f=self.fixture;self.f.install();self.destination=self.f.root/'common-profile'
 def tearDown(self):self.fixture.tearDown()
 def bind(self):return CommonProfile.bind(self.f.store,CLI,self.f.adapter,self.destination)
 def test_binds_original_full_measurement_without_remeasurement(self):
  before={str(p):p.read_bytes() for p in self.f.root.rglob('*') if p.is_file()};result=self.bind()
  self.assertFalse(result['production_enabled']);self.assertEqual(len(result['evidence_sha256']),14) # fixture reuses CLI for BitCrack
  self.assertEqual(self.bind(),result);self.assertEqual(self.destination.stat().st_mode&0o777,0o600)
  self.assertTrue(self.destination.read_text().startswith('OPV-RUN-PROFILE-1\n'));self.assertIn(str(self.f.evidence),result['evidence_sha256'])
  for path,contents in before.items():self.assertEqual(Path(path).read_bytes(),contents)
 def test_changed_per_run_record_refused_preserving_bound_profile(self):
  self.bind();before=self.destination.read_bytes();(self.f.root/'measure-0-vanity-cli/result.json').write_text('{}')
  with self.assertRaises(Exception):self.bind()
  self.assertEqual(self.destination.read_bytes(),before)
 def test_changed_previous_profile_or_cli_does_not_replace_binding(self):
  self.bind();before=self.destination.read_bytes();candidate=self.f.store/(self.f.record['binding_sha256']+'.json');candidate.write_text('{}')
  with self.assertRaises(Exception):self.bind()
  self.assertEqual(self.destination.read_bytes(),before)
 def test_unsafe_existing_destination_refused(self):
  self.destination.symlink_to(self.f.evidence)
  with self.assertRaises(Exception):self.bind()
  self.assertTrue(self.destination.is_symlink())
if __name__=='__main__':unittest.main(verbosity=2)
