from pathlib import Path
import argparse,subprocess,json,time,sys
parser=argparse.ArgumentParser(description='Run the recovered Windows host cases.')
parser.add_argument('--build-dir',type=Path,default=Path(__file__).resolve().parent/'.build')
root=parser.parse_args().build_dir.resolve()
cases={'old':['oom-poll','oom-recv','oom-sent','oom-error','oom-clear','oom-connected','oom-dns','oom-accept','queue-no-refill','queue-refill'],'stable':['oom-poll','oom-recv','oom-sent','oom-error','oom-connected','oom-dns','queue-no-refill','queue-refill']}
results=[]
for version,modes in cases.items():
 for mode in modes:
  start=time.monotonic()
  p=subprocess.run([str(root/(version+'_harness.exe')),mode],capture_output=True,text=True,timeout=8,creationflags=subprocess.CREATE_NO_WINDOW)
  is_old_oom=version=='old' and mode.startswith('oom-')
  expected=(p.returncode & 0xffffffff)==0xc0000005 if is_old_oom else p.returncode==0
  results.append({'version':version,'case':mode,'returncode':p.returncode,'returncode_hex':hex(p.returncode&0xffffffff),'expected_observation':expected,'stdout':p.stdout.strip(),'stderr':p.stderr.strip(),'elapsed_seconds':round(time.monotonic()-start,3)})
  print(version,mode,'expected_observation='+str(expected),hex(p.returncode&0xffffffff),p.stdout.strip())
report={'recorded_at':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'all_expected_observations':all(x['expected_observation'] for x in results),'cases':results,'scope':'Deterministic host reproduction using exact selected source function bodies. Expected old OOM access violation and queue self-blocking are defects, not successful production behavior. No hardware test, deployment, public fork, issue, PR, or multi-day test.'}
(root/'test-results.json').write_text(json.dumps(report,indent=2))
sys.exit(0 if report['all_expected_observations'] else 1)
