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
const nodes = Object.fromEntries(['form','ssid','pass','net','save','result','saved','finish'].map(k => [k,element()]));
let names = ['<img src=x onerror=alert(1)>','Home'], status = 6, sent;
const context = vm.createContext({document:{getElementById:id=>nodes[id],createElement:element},confirm:()=>true,setTimeout:()=>{},fetch:async(url,options)=>{
  if(url==='/configure')sent=options;
  return {ok:true,json:async()=>url==='/saved'?names:url==='/networks'?[]:status};
}});
const flush = async()=>{for(let i=0;i<15;i++)await Promise.resolve()};
(async()=>{
  vm.runInContext(script,context);await flush();
  assert.equal(nodes.saved.children[0].children[0].textContent,names[0]);
  assert.equal(nodes.finish.disabled,false);
  nodes.saved.children[0].children[1].onclick();await flush();
  assert.deepEqual(JSON.parse(sent.body),{ssid:names[0],password:'',remove:true});
  assert.equal(sent.headers['X-Setup-Token'],'test-token');
  assert.match(nodes.result.textContent,/已满 5 组/);
  nodes.finish.onclick();await flush();
  assert.equal(JSON.parse(sent.body).finish,true);
  names=[];status=5;await vm.runInContext('poll()',context);await flush();
  assert.equal(nodes.saved.children.length,0);assert.equal(nodes.finish.disabled,true);
  status=3;nodes.pass.value='secret';await vm.runInContext('poll()',context);
  assert.equal(nodes.pass.value,'');
  for (const [code, expected] of [[7,/认证失败/],[8,/未找到/],[9,/安全模式/],[10,/获取 IP 超时/]]) {
    status=code;await vm.runInContext('poll()',context);
    assert.match(nodes.result.textContent,expected);
    assert.equal(nodes.save.disabled,false);
  }
  console.log('Provisioning page: PASS (safe SSID text, deletion, finish, capacity, password clearing)');
})().catch(e=>{console.error(e);process.exitCode=1});
