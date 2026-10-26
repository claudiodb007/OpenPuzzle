#!/usr/bin/env python3
"""Measure the validated public-target Vanity CLI without rebuilding or installing."""
import argparse,json,os,re,shutil,sys,tempfile
from datetime import datetime,timezone
from pathlib import Path
import Evidence as e
import Measure as m
import Support as s
ROOT=Path(__file__).resolve().parent
VERSION='0.7.0'
LIMITATION='BitCrack has an unresolved unaligned-tail overrun finding. This comparison uses aligned intervals and does not establish general exact-range correctness for BitCrack.'
def find_bitcrack(report,explicit):
    if explicit:return Path(explicit).expanduser().resolve()
    cache=report/'build/CMakeCache.txt'
    values=[line.split('=',1)[1] for line in cache.read_text().splitlines() if line.startswith('OPENPUZZLE_BUNDLED_CUDA_ENGINE:')]
    e.require(len(values)==1 and Path(values[0]).is_file(),'Prior bundled CUDA engine missing; use --bitcrack PATH')
    return Path(values[0]).resolve()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',action='store_true');p.add_argument('--prior-report',type=Path,default=Path.home()/'OpenPuzzle/relatorios/vanity-http/http-hq6heija');p.add_argument('--bitcrack');args=p.parse_args()
    e.require(sys.platform=='linux' and os.uname().machine=='x86_64' and os.geteuid()!=0,'Executa em Linux x86_64, como utilizador normal, sem sudo')
    os.umask(0o077);prior=args.prior_report.expanduser().resolve()
    original,uuid,suites,preserved=e.verify_prior(prior);original['bitcrack']=find_bitcrack(prior,args.bitcrack);preserved[str(original['bitcrack'])]=s.sha(original['bitcrack'])
    parent=Path.home()/'OpenPuzzle/relatorios/vanity-cli-performance';parent.mkdir(parents=True,exist_ok=True);report=Path(tempfile.mkdtemp(prefix='measurement-',dir=parent));(report/'bin').mkdir();(report/'home').mkdir()
    print('OpenPuzzle — medição da CLI Vanity, preview '+VERSION+'\nRelatório: '+str(report),flush=True)
    summary=dict(version=VERSION,status='preparing',prior_report=str(prior),prior_linux_suites=suites,production_enabled=False,hardware_requested=args.run,results=[],profiles_changed=False)
    try:
        for filename in ('HTTP_SOURCE.json','ENGINE_SOURCE.json','KNOWN_BASELINE_LIMITATION.json'):
            shutil.copy2(ROOT/filename,report/filename)
        env=os.environ.copy()
        for k in list(env):
            if k.startswith(('OPENPUZZLE_','OPV_','CUDA_')) or k in ('LD_PRELOAD','PYTHONPATH'):env.pop(k,None)
        env.update(HOME=str(report/'home'),XDG_CONFIG_HOME=str(report/'home/.config'),XDG_DATA_HOME=str(report/'home/.local/share'),CUDA_VISIBLE_DEVICES=uuid,CUDA_DEVICE_ORDER='PCI_BUS_ID',CUDA_CACHE_PATH=str(report/'cuda-cache'))
        paths={key:report/'bin'/name for key,name in dict(cli='openpuzzle',adapter='openpuzzle-vanity-adapter',engine='vanity-target',bitcrack='cuBitCrack').items()}
        for key,source in original.items():
            shutil.copy2(source,paths[key]);e.require(s.sha(source)==s.sha(paths[key]),'Executable copy differs')
        e.require(s.run_capture([paths['engine'],'--identity'],env=env).strip()=='OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1','Wrong target engine identity')
        e.require(s.run_capture([paths['bitcrack'],'--openpuzzle-engine-version'],env=env).strip()=='OpenPuzzle-BitCrack 1.0.0 CUDA protocol 2','Wrong BitCrack identity')
        cards=[row for row in s.smi(['uuid','name','driver_version']) if row[0]==uuid];e.require(len(cards)==1 and cards[0][1]=='NVIDIA GeForce RTX 4070 SUPER','Prior RTX 4070 SUPER not uniquely available')
        _,name,driver=cards[0]
        inventory=s.run_capture([paths['bitcrack'],'--list-devices'],env=env)
        e.require(re.findall(r'^\s*ID:\s*(\d+)\s*$',inventory,re.M)==['0'] and re.findall(r'^\s*Name:\s*(.+?)\s*$',inventory,re.M)==[name],'BitCrack did not isolate the bound GPU')
        telemetry=s.telemetry(uuid);limit=telemetry['power_limit_w']
        binary_pins={str(p):s.sha(p) for p in paths.values()}
        executables={key:dict(path=str(path),sha256=s.sha(path)) for key,path in paths.items()}
        baseline=dict(gpu=name,uuid=uuid,driver=driver,power_limit_w=limit,telemetry=telemetry,executables=executables,bitcrack_parameters=e.PARAMS,vanity_grid=e.GRID,count=e.MEASURE_COUNT,prior_summary_sha256=s.sha(prior/'SUMMARY.json'))
        s.write_json(report/'baseline.json',baseline)
        if not args.run:
            summary['status']='prepared_measurement_pending';print('Relatório anterior, executáveis e GPU verificados. Usa --run para medir.',flush=True);return
        s.require_idle(uuid)
        print(f'{suites} suites anteriores e validação HTTP/GPU confirmadas; nada recompilado.\nTrês pares alternados; prevê cerca de 15–25 minutos. Limite mantido: {limit:.1f} W.',flush=True)
        print('Vanity: tempo total da CLI + HTTP local + checkpoints. BitCrack: tempo total do motor.\nA comparação usa intervalos alinhados; a falha anterior do BitCrack em finais não alinhados continua pendente.',flush=True)
        summary['results'].append(m.bit_fixture(report,'correct-bitcrack',paths,e.COMMON_BATCH,env,uuid,limit,correctness=True))
        summary['results'].append(m.bit_fixture(report,'warmup-bitcrack',paths,e.WARMUP_COUNT,env,uuid,limit))
        summary['results'].append(m.cli_fixture(report,'warmup-vanity-cli',paths,e.WARMUP_COUNT,env,uuid,limit))
        measured=[]
        for pair in range(3):
            order=('bitcrack','vanity-cli') if pair%2==0 else ('vanity-cli','bitcrack')
            for kind in order:
                fn=m.bit_fixture if kind=='bitcrack' else m.cli_fixture
                row=fn(report,f'measure-{pair}-{kind}',paths,e.MEASURE_COUNT,env,uuid,limit,pair=pair);measured.append(row);summary['results'].append(row)
                e.verify_preserved(preserved);e.verify_preserved(binary_pins)
        metrics,gain=e.metrics(measured);summary.update(metrics=metrics,vanity_cli_gain_percent=gain)
        # Verify the saved per-run records before sealing immutable measurement evidence.
        for row in summary['results']:e.require(e.read_json(report/row['label']/'result.json')==row,'Stored measurement result changed')
        evidence=dict(version=VERSION,status='completed',created_utc=datetime.now(timezone.utc).isoformat(),production_enabled=False,gpu=name,uuid=uuid,driver=driver,power_limit_w=limit,grid=e.GRID,threads=256,points=1024,count=e.MEASURE_COUNT,executables=executables,measurements=measured,metrics=metrics,vanity_cli_gain_percent=gain,comparison_basis='BitCrack whole engine process vs Vanity whole CLI process with loopback HTTP and durable checkpoints; identical aligned interval and one absent compressed P2PKH target',baseline_limitation=LIMITATION,prior_report=str(prior),prior_summary_sha256=s.sha(prior/'SUMMARY.json'),source_checksums=dict(path=str(report/'HTTP_SOURCE.json'),sha256=s.sha(report/'HTTP_SOURCE.json')))
        evidence_path=report/'MEASUREMENT.json';e.publish_private(evidence_path,evidence)
        if not all(item['stable'] for item in metrics.values()):
            summary['status']='measurements_unstable_profile_not_created';print('Dispersão superior a 3%: medições guardadas; nenhum perfil criado.',flush=True)
        else:
            profile=e.profile(evidence,evidence_path,paths);profile_path=report/'vanity-cli-measured.profile.json';e.publish_private(profile_path,profile)
            e.require(e.read_json(profile_path)==profile,'Profile readback differs');summary.update(status='completed',profile=str(profile_path),profile_sha256=s.sha(profile_path))
        print('\n===== MEDIÇÃO DA CLI VANITY CONCLUÍDA =====',flush=True)
        for kind in ('bitcrack','vanity-cli'):print(f'{kind}: {metrics[kind]["median_mkeys"]:.2f} MKey/s; dispersão {metrics[kind]["spread_percent"]:.2f}%')
        print(f'Variação Vanity CLI: {gain:+.1f}%',flush=True)
        if 'profile' in summary:print(f'Perfil candidato: {summary["profile"]}\nVelocidade de planeamento (-3%): {profile["planning_mkeys"]:.2f} MKey/s',flush=True)
        print('Perfil independente para a próxima integração; ainda não importado pela UI/CLI instalada.\nSem recompilação, instalação, alteração de potência/ventoinhas ou pedidos de produção.',flush=True)
        return report
    except BaseException as exc:summary.update(status='cancelled' if isinstance(exc,KeyboardInterrupt) else 'failed',error=str(exc));raise
    finally:
        try:e.verify_preserved(preserved)
        except Exception as exc:summary.update(status='failed',preservation_error=str(exc));raise
        finally:s.write_json(report/'SUMMARY.json',summary);print('Relatório preservado:',report,flush=True)
if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('Cancelado; dados preservados.',file=sys.stderr);sys.exit(130)
    except Exception as exc:print('Interrompido:',exc,file=sys.stderr);sys.exit(1)
