import datetime
import json
import unittest
from pathlib import Path
import jinja2
import yaml
ROOT = Path(__file__).resolve().parents[2]
CONFIG = yaml.safe_load((ROOT / 'docs/home-assistant-bms-controls.yaml').read_text(encoding='utf-8'))
class ActualTemplates(unittest.TestCase):
    def setUp(self):
        self.env=jinja2.Environment()
        self.env.filters['to_json']=json.dumps
        self.env.globals.update(state_attr=lambda entity,key: 'a'*32 if key=='session' else None,
          now=lambda:datetime.datetime(2026,10,22,13,tzinfo=datetime.timezone.utc),as_timestamp=lambda v:v.timestamp())
    def render(self,t,**v):return self.env.from_string(t).render(**v).strip()
    def test_actual_states_and_explicit_manual_commands(self):
        for s in CONFIG['switch']:
            d='charge' if '_charge_' in s['unique_id'] else 'discharge'
            self.assertEqual(s['state_topic'],f'SolarBatteryTelemetry/bms/{d}_enabled')
            for value in ('true','false'):
                self.assertEqual(self.render(s['value_template'],value_json=value=='true'),value)
                p=self.render(s['command_template'],value=value);c=json.loads(p)
                self.assertEqual(set(c),{'state','session','expires_at','request_id'})
                self.assertEqual(c['state'],value=='true');self.assertEqual(c['session'],'a'*32)
                self.assertLessEqual(len(p.encode()),256)
            self.assertFalse(s['retain']);self.assertFalse(s['optimistic'])
    def test_no_historical_or_pending_gray_dependency(self):
        self.assertNotIn('acknowledged',json.dumps(CONFIG))
        for s in CONFIG['switch']:
            check=next(a['value_template'] for a in s['availability'] if a['topic'].endswith('/control'))
            for reason,enabled,expected in [('ok',True,'available'),('busy',True,'available'),('disabled',False,'available'),('stale',True,'available')]:
                self.assertEqual(self.render(check,value_json={'reason':reason,'enabled':enabled,'session':'a'*32,'permissions_known':True}),expected)
    def test_old_result_is_neutral_after_new_session(self):
        check=next(s['value_template'] for s in CONFIG['sensor'] if s['unique_id']=='sbt_bms_control_result')
        for session,expected in [('b'*32,'idle: no_request'),('a'*32,'acknowledged: ok')]:
            self.assertEqual(self.render(check,value_json={'phase':'acknowledged','reason':'ok','session':session}),expected)
    def test_gate_uses_same_setting_over_nonretained_mqtt(self):
        gate=yaml.safe_load((ROOT/'docs/home-assistant-bms-gate.yaml').read_text(encoding='utf-8'))[0]['switch'][0]
        for action,state in [('turn_on',True),('turn_off',False)]:
            a=gate[action][0];self.assertEqual(a['action'],'mqtt.publish');self.assertFalse(a['data']['retain'])
            self.assertEqual(a['data']['topic'],'SolarBatteryTelemetry/bms/control_enabled/set')
            c=json.loads(self.render(a['data']['payload']));self.assertEqual(c['state'],state)
            self.assertEqual(set(c),{'state','session','expires_at','request_id'})
if __name__=='__main__':unittest.main()
