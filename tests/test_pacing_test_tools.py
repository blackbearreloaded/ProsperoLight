# ps5-native-app-boilerplate - Repeatable host pacing test checks.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PacingTestTools(unittest.TestCase):
    def test_finished_session_key(self):
        spec = importlib.util.spec_from_file_location('collector', ROOT / 'tools/pacing-test/collect.py')
        collector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(collector)
        trace = ('pyrowave-decisions-mode2.csv', dict(type='file', modify='1', size='200'))
        live = ('debug-trace.txt', dict(type='file', modify='1', size='1000'))
        key = collector.completion_key([trace, live])
        live[1]['modify'] = '2'
        self.assertEqual(key, collector.completion_key([trace, live]))
        trace[1]['modify'] = '3'
        self.assertNotEqual(key, collector.completion_key([trace, live]))

    @unittest.skipUnless(shutil.which('node'), 'Node is needed for host scene execution')
    def test_scene_schedule_and_static_freeze(self):
        html = (ROOT / 'tools/pacing-test/index.html').read_text()
        script = html.split('<script>', 1)[1].split('</script>', 1)[0]
        harness = r'''
const vm=require('vm'),assert=require('assert');
let paints=0,queue=[],uploads=[];
const context={fillRect(){paints++},strokeRect(){},drawImage(){paints++},fillText(){paints++}};
const fields={stimulus:{value:'reference'},fps:{value:'45'},codec:{value:'HEVC'},mode:{value:'Paced+VRR'},hdr:{checked:true},vsync:{checked:true},start:{},save:{},result:{},scene:{getContext(){return context}}};
const document={hidden:false,fullscreenElement:null,body:{classList:{add(){},remove(){}}},getElementById(id){return fields[id]},createElement(){return {getContext(){return context}}},addEventListener(){}};
document.documentElement={async requestFullscreen(){document.fullscreenElement=this}};
const sandbox={async fetch(url,options){uploads.push({url,data:JSON.parse(options.body)});return {ok:true,async json(){return {id:'test-saved'}}}},document,navigator:{userAgent:'test'},innerWidth:3840,innerHeight:2160,devicePixelRatio:1,performance:{now(){return 0}},requestAnimationFrame(f){queue.push(f)},Date,console,alert(){throw Error('Unexpected validation error')}};
vm.createContext(sandbox);
vm.runInContext(SCRIPT,sandbox);
(async()=>{
 assert.equal(vm.runInContext('phaseAt(29).motion',sandbox),true);
 assert.equal(vm.runInContext('phaseAt(30).motion',sandbox),false);
 assert.equal(vm.runInContext('phaseAt(50).motion',sandbox),true);
 assert.equal(vm.runInContext('phaseAt(90)',sandbox),null);
 await vm.runInContext('start()',sandbox);
 function tick(t){queue.shift()(t)}
 tick(0);tick(1000);tick(30000);
 const before=paints;tick(35000);tick(49000);
 assert.equal(paints,before,'Static scene must not repaint or change counters');
 tick(50000);assert(paints>before);
 tick(90000);
 assert.equal(vm.runInContext('running',sandbox),false);
 assert.equal(vm.runInContext('report.aborted',sandbox),false);
 assert.equal(vm.runInContext('report.settings.fps',sandbox),45);
 assert.equal(fields.save.disabled,false);
 assert.equal(vm.runInContext('report.settings.scene',sandbox),'reference');
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(uploads.length,1);
 assert.equal(uploads[0].url,'/api/reports');
 assert.equal(uploads[0].data.aborted,false);
 assert(fields.result.textContent.includes('test-saved'));
})().catch(e=>{console.error(e);process.exitCode=1});
'''.replace('SCRIPT', json.dumps(script))
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'scene-test.cjs'
            source.write_text(harness)
            subprocess.run(['node', str(source)], check=True, timeout=20)


if __name__ == '__main__':
    unittest.main()
