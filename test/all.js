#!/usr/bin/env node
/**
 * flowjs — comprehensive test suite for all parser features.
 */
'use strict';
const f = require(__dirname + '/../build/Release/flowjs_native');
let pass = 0, fail = 0;

function test(label, src, check) {
  try {
    const ast = JSON.parse(f.parse(src));
    if (check && typeof check === 'function') {
      if (!check(ast)) { throw new Error('check failed: ' + JSON.stringify(ast).substring(0, 80)); }
    }
    console.log('✓', label);
    pass++;
  } catch (e) {
    console.log('✗', label, '—', e.message.substring(0, 70));
    fail++;
  }
}

console.log('=== BASELINE (21) ===');
test('var',         'var x = 1;');
test('let',         'let x = 1;');
test('const',       'const x = 1;');
test('function',    'function f() { return 1; }');
test('arrow',       'const f = (x) => x + 1;');
test('template',    'const m = `hello`;');
test('optional',    'const v = obj?.prop;');
test('class',       'class A { constructor() {} }');
test('ternary',     'const t = x > 0 ? 1 : 2;');
test('object',      'const o = { a: 1, b: 2 };', ast => ast.body[0].declarations[0].init.properties.length === 2);
test('labelled',    'foo: while(true) { break foo; }');
test('async fn',    'async function foo() { return 1; }');
test('destr params','function f([a], {b}) { return a + b; }');
test('destr vars',  'var [a, b] = [1, 2];');
test('keyword keys','var x = { if: 42, true: false };');
test('computed keys','var x = { ["a" + "b"]: 1 };');
test('spread',      'var x = { ...a, b: 1 };');
test('for of',      'for (let x of [1,2,3]) { x; }');
test('try catch',   'try { 1; } catch (e) { 2; } finally { 3; }');
test('yield',       'function* f() { yield 1; }');
test('switch',      'switch (x) { case 1: break; case 2: break; default: break; }');

console.log('\n=== NEW FIXES (19) ===');
test('new Foo',     'new Foo()', ast => ast.body[0].expression.type === 'NewExpression');
test('new args',    'new Foo(1, 2)', ast => ast.body[0].expression.arguments.length === 2);
test('async arrow', 'const f = async () => 1;', ast => ast.body[0].declarations[0].init.type === 'ArrowFunctionExpression');
test('async block', 'const f = async () => { return 1; };');
test('async param', 'const f = async (x) => x;');
test('import',      'import foo from "bar";', ast => ast.body[0].type === 'ImportDeclaration');
test('import side', 'import "foo";');
test('regex stmt',  '/abc/gi');
test('regex var',   'var x = /abc/;');
test('arrow default','const f = (x = 1) => x;');
test('arrow rest',  'const f = (...args) => args;');
test('static class','class A { static method() {} }');
test('getter',      'class A { get x() { return 1; } }');
test('setter',      'class A { set x(v) { this._x = v; } }');
test('typeof',      'typeof x');
test('delete',      'delete obj.prop');
test('void',        'void 0');
test('await',       'const f = async () => { await 1; };');
test('export fn',   'export default function() {}');

console.log('\n' + '='.repeat(40));
console.log(pass + '/' + (pass + fail) + ' passed');
process.exit(fail > 0 ? 1 : 0);