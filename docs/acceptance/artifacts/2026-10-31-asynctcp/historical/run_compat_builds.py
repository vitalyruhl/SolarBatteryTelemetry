from pathlib import Path
import json,subprocess,time,traceback
ROOT=Path(__file__).resolve().parent
CONSUMER=ROOT/'sbt-compat-consumer'
PIO=Path(r'C:\Users\Vital\.platformio\penv\Scripts\platformio.exe')
status={'state':'running','started_at':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'stages':[]}
statusfile=ROOT/'compat-build-status.json'
def save():statusfile.write_text(json.dumps(status,indent=2))
save()
try:
 for env in ['usb','compat_async35_usb','compat_async35_ota']:
  status['active_environment']=env;save()
  log=ROOT/('compat-build-'+env+'.log');start=time.monotonic()
  with log.open('w',encoding='utf-8') as out:
   proc=subprocess.run([str(PIO),'run','-d',str(CONSUMER),'-e',env],cwd=CONSUMER,stdout=out,stderr=subprocess.STDOUT,timeout=1200,creationflags=subprocess.CREATE_NO_WINDOW)
  status['stages'].append({'environment':env,'returncode':proc.returncode,'log':str(log),'elapsed_seconds':round(time.monotonic()-start,1)})
  save()
  if proc.returncode:status['state']='failed';break
 else:status['state']='succeeded'
except BaseException as exc:
 status['state']='runner_error';status['error']=str(exc)
finally:
 status['ended_at']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime());save()
