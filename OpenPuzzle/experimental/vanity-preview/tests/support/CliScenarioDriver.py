import LocalScenarios as m
import Support as s
class CliScenarios(m.Scenarios):
    def __init__(self,*args,profile,**kwargs):super().__init__(*args,**kwargs);self.profile=profile
    def setup(self,*args):
        d,native=super().setup(*args)
        command=[self.client,'vanity-preview','local-test','--profile',self.profile]
        for key,index in [('--root',4),('--controls',5),('--keys',6),('--expected',7),('--start',8),('--count',9),('--grid',10),('--scenario',13)]:command.extend([key,native[index]])
        command.append('--enable-experimental-vanity-local');s.write_json(d/'argv.json',[str(x) for x in command]);return d,command
