// Run with Node.js; exercise the actual embedded provisioning script without a browser.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../main/fmo_provision.c'), 'utf8');
function literal(name) {
  const block = source.split(`static const char ${name}[] =`)[1].split('\n\n')[0];
  return [...block.matchAll(/^"(?:[^"\\]|\\.)*"/gm)].map(m => JSON.parse(m[0])).join('');
}
// page and page_end are adjacent declarations; extract each precisely.
const first = source.match(/static const char page\[\] =([\s\S]*?);\nstatic const char page_end/)[1];
const prefix = [...first.matchAll(/^"(?:[^"\\]|\\.)*"/gm)].map(m => JSON.parse(m[0])).join('');
const script = (prefix + 'test-token' + literal('page_end')).match(/<script>([\s\S]*?)<\/script>/)[1];
function element() { return {value:'',textContent:'',disabled:false,children:[],append(...items){this.children.push(...items)},replaceChildren(){this.children=[]}}; }
const nodes = Object.fromEntries(['form','ssid','pass','net','save','result','saved','finish','host','port','fmoform','fmosave','fmotest'].map(k => [k,element()]));
let targets={host:'fmo.local',port:80,ssid:'Home',networks:[{ssid:'Home',host:'home.local',port:8080},{ssid:'Office',host:'office.local',port:9090}]};
let names = ['<img src=x onerror=alert(1)>','Home'], status = 6, sent;
const context = vm.createContext({document:{getElementById:id=>nodes[id],createElement:element},confirm:()=>true,setTimeout:()=>{},fetch:async(url,options)=>{
  if(options?.method==='POST')sent={...options,url};
  return {ok:true,json:async()=>url==='/saved'?names:url==='/networks'?[]:url==='/fmo'?targets:status};
}});
const flush = async()=>{for(let i=0;i<15;i++)await Promise.resolve()};
(async()=>{
  vm.runInContext(script,context);await flush();
  assert.equal(nodes.saved.children[0].children[0].textContent,names[0]);
  assert.equal(nodes.finish.disabled,false);
  assert.equal(nodes.ssid.value,'Home');assert.equal(nodes.host.value,'home.local');assert.equal(nodes.port.value,8080);
  nodes.net.value='Office';nodes.net.onchange();assert.equal(nodes.host.value,'office.local');assert.equal(nodes.port.value,9090);
  nodes.ssid.value='New';nodes.ssid.onchange();assert.equal(nodes.host.value,'fmo.local');assert.equal(nodes.port.value,80);
  nodes.ssid.value='Home';nodes.ssid.onchange();
  nodes.saved.children[0].children[1].onclick();await flush();
  assert.deepEqual(JSON.parse(sent.body),{ssid:names[0],password:'',remove:true});
  assert.equal(sent.headers['X-Setup-Token'],'test-token');
  assert.match(nodes.result.textContent,/已满 5 组/);
  nodes.finish.onclick();await flush();
  assert.equal(JSON.parse(sent.body).finish,true);assert.equal(JSON.parse(sent.body).ssid,'Home');
  names=[];status=5;await vm.runInContext('poll()',context);await flush();
  assert.equal(nodes.saved.children.length,0);assert.equal(nodes.finish.disabled,true);
  status=3;nodes.pass.value='secret';await vm.runInContext('poll()',context);
  assert.equal(nodes.pass.value,'');
  assert.equal(nodes.host.value,'home.local');
  nodes.host.value='192.168.1.2';nodes.port.value='8080';
  status=11;nodes.fmosave.onclick();await flush();
  assert.equal(sent.url,'/fmo');
  assert.deepEqual(JSON.parse(sent.body),{ssid:'Home',host:'192.168.1.2',port:8080,probe:false});
  assert.equal(sent.headers['X-Setup-Token'],'test-token');
  assert.match(nodes.result.textContent,/地址已保存/);
  status=13;nodes.fmotest.onclick();await flush();
  assert.equal(JSON.parse(sent.body).probe,true);
  assert.match(nodes.result.textContent,/端口可达/);
  for(const [code,expected] of [[12,/Wi-Fi 尚未连接/],[14,/连接失败/]]){
    status=code;await vm.runInContext('poll()',context);assert.match(nodes.result.textContent,expected);
  }
  for (const [code, expected] of [[7,/认证失败/],[8,/未找到/],[9,/安全模式/],[10,/获取 IP 超时/]]) {
    status=code;await vm.runInContext('poll()',context);
    assert.match(nodes.result.textContent,expected);
    assert.equal(nodes.save.disabled,false);
  }
  status=6;nodes.ssid.value='New';nodes.ssid.onchange();nodes.pass.value='new-secret';nodes.host.value='new.local';nodes.port.value=1234;
  nodes.form.onsubmit({preventDefault(){}});await flush();
  assert.equal(sent.url,'/configure');assert.deepEqual(JSON.parse(sent.body),{ssid:'New',password:'new-secret',host:'new.local',port:1234});
  nodes.fmosave.onclick();assert.match(nodes.result.textContent,/尚未保存/);
  nodes.ssid.value='Office';nodes.ssid.onchange();assert.equal(nodes.host.value,'office.local');assert.equal(nodes.pass.value,'');
  console.log('Provisioning page: PASS (safe SSID text, deletion, finish, capacity, password clearing)');
})().catch(e=>{console.error(e);process.exitCode=1});
