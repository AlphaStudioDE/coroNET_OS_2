// Exercises the actual inline web functions with controlled asynchronous replies.
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const html = fs.readFileSync(path.join(root, 'web/index.html'), 'utf8');
const scripts = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g)].map(match => match[1]);
scripts.forEach((source, index) => new vm.Script(source, { filename: `web-inline-${index}.js` }));
console.log(`Web syntax: ${scripts.length} inline script(s) compiled`);
const functionLine = name => {
  const line = html.split(/\r?\n/).find(value => value.startsWith(`function ${name}(`) || value.startsWith(`async function ${name}(`));
  assert.ok(line, `Missing ${name}`);
  return line;
};

const replies = [];
const colors = {};
const context = vm.createContext({
  ledCat: 0,
  ledCatalog: [],
  ledCatalogRequest: 0,
  settings: { accentHueDegrees: 0 },
  request: url => new Promise(resolve => replies.push({ url, resolve })),
  renderLed() {},
  toast(message) { throw new Error(message); },
  matchMedia: () => ({ matches: false }),
  document: { documentElement: { style: { setProperty(key, value) { colors[key] = value; } } } },
});
vm.runInContext(functionLine('loadLedCatalog'), context);
vm.runInContext(functionLine('applyTheme'), context);
const idleRequest = vm.runInContext('loadLedCatalog()', context);
context.ledCat = 1;
const printRequest = vm.runInContext('loadLedCatalog()', context);
replies[1].resolve({ animations: ['PRINT catalogue'] });
await printRequest;
replies[0].resolve({ animations: ['IDLE catalogue'] });
await idleRequest;
assert.equal(context.ledCat, 1);
assert.equal(context.ledCatalog[0], 'PRINT catalogue');
console.log('PASS: late IDLE catalogue cannot replace PRINT');
const oldRequest = vm.runInContext('loadLedCatalog()', context);
const newRequest = vm.runInContext('loadLedCatalog()', context);
replies[3].resolve({ animations: ['Newest PRINT'] });
await newRequest;
replies[2].resolve({ animations: ['Older PRINT'] });
await oldRequest;
assert.equal(context.ledCatalog[0], 'Newest PRINT');
console.log('PASS: newest catalogue wins within the same category');
vm.runInContext('applyTheme()', context);
assert.match(colors['--accent'], /^hsl\(0 /);
console.log('PASS: accent hue zero remains red');
