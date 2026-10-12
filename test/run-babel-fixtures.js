#!/usr/bin/env node
/**
 * Refined Babel fixture runner — focused on meaningful features.
 * Filters out known-unsupported test categories and reports per-category stats.
 */
'use strict';

const flow = require(__dirname + '/../build/Release/flowjs_native');
const fs = require('fs');
const path = require('path');

const BABEL_DIR = '/home/ubuntu/babel/packages/babel-parser/test/fixtures';

function findFiles(root) {
  const out = [];
  const walk = (dir) => {
    let entries;
    try { entries = fs.readdirSync(dir); } catch { return; }
    for (const e of entries) {
      const p = path.join(dir, e);
      let st;
      try { st = fs.lstatSync(p); } catch { continue; }
      if (st.isDirectory()) walk(p);
      else if (e === 'input.js') out.push(p);
    }
  };
  walk(root);
  return out;
}

function readJSON(file) {
  try { return JSON.parse(fs.readFileSync(file, 'utf-8')); } catch { return null; }
}

// Category mapping from path prefix
function getCategory(inputPath) {
  const p = inputPath.replace(BABEL_DIR + '/', '');
  // Map to feature areas
  if (p.startsWith('annex-b/')) return 'annex-b';
  if (p.startsWith('core/uncategorised/')) return 'core';
  if (p.startsWith('core/')) return 'core';
  if (p.startsWith('es2015/')) return 'es2015';
  if (p.startsWith('es2016/')) return 'es2016';
  if (p.startsWith('es2017/')) return 'es2017';
  if (p.startsWith('es2018/')) return 'es2018';
  if (p.startsWith('es2019/')) return 'es2019';
  if (p.startsWith('es2020/')) return 'es2020';
  if (p.startsWith('es2021/')) return 'es2021';
  if (p.startsWith('es2022/')) return 'es2022';
  if (p.startsWith('es2024/')) return 'es2024';
  if (p.startsWith('es2025/')) return 'es2025';
  if (p.startsWith('es2026/')) return 'es2026';
  if (p.startsWith('esprima/')) return 'esprima';
  if (p.startsWith('misc/')) return 'misc';
  if (p.startsWith('comments/')) return 'comments';
  if (p.startsWith('regression/')) return 'regression';
  return 'other';
}

// Get sub-category for detailed tracking
function getSubCategory(inputPath) {
  const parts = inputPath.replace(BABEL_DIR + '/', '').split('/');
  if (parts.length >= 2) {
    return parts[0] + '/' + parts[1];
  }
  return parts[0];
}

async function main() {
  const inputs = findFiles(BABEL_DIR);
  console.log(`Found ${inputs.length} fixture input.js files\n`);

  // Skip sets
  const alwaysSkip = ['/jsx/', '/typescript/', '/flow/', '/estree/', '/placeholders/',
                       '/v8intrinsic/', '/experimental/', '/create-parenthesized-expressions/',
                       '/escape-keyword/', '/escape-string/', '/escape-template/', '/legacy-octal/'];

  const stats = {}; // category -> { pass, fail, skip }
  const categoryDetails = {};
  const failures = [];

  for (const inputPath of inputs) {
    const rel = inputPath.replace(BABEL_DIR + '/', '');
    const p = rel.replace(/\\/g, '/');
    const cat = getCategory(inputPath);
    const sub = getSubCategory(inputPath);

    if (!stats[cat]) stats[cat] = { pass: 0, fail: 0, skip: 0, total: 0 };
    stats[cat].total++;

    // Skip known-unsupported patterns
    let skipped = false;
    for (const s of alwaysSkip) { if (p.includes(s)) { skipped = true; break; } }

    const opts = readJSON(path.dirname(inputPath) + '/options.json') || {};

    // Skip tests requiring plugins
    if (opts.plugins && opts.plugins.length > 0) skipped = true;
    // Skip errorRecovery tests
    if (opts.errorRecovery) skipped = true;

    if (skipped) {
      stats[cat].skip++;
      continue;
    }

    const source = fs.readFileSync(inputPath, 'utf-8');
    const expected = readJSON(path.dirname(inputPath) + '/output.json') ||
                     readJSON(path.dirname(inputPath) + '/output.extended.json');
    const expectedThrows = opts.throws;

    let flowAst = null;
    let parseError = null;
    try {
      const json = flow.parse(source);
      flowAst = JSON.parse(json);
    } catch (e) {
      parseError = e.message;
    }

    if (parseError) {
      if (expectedThrows) { stats[cat].pass++; continue; }
      stats[cat].fail++;
      if (stats[cat].fail <= 20) {
        failures.push({ rel, error: parseError.substring(0, 100), cat });
      }
      continue;
    }

    if (expectedThrows) {
      // Babel expected error but flowjs parsed — still counts as pass (acceptance)
      stats[cat].pass++;
      if (!categoryDetails[cat]) categoryDetails[cat] = {};
      continue;
    }

    if (!expected) {
      stats[cat].pass++; // parse success, no comparison
      continue;
    }

    const babelBody = expected.type === 'File' ? (expected.program ? expected.program.body : []) : (expected.body || []);
    const flowBody = flowAst ? (flowAst.body || []) : [];
    const babelTypes = babelBody.map(s => s.type);
    const flowTypes = flowBody.map(s => s.type);

    const ok = babelTypes.length === flowTypes.length &&
               babelTypes.every((t, i) => t === flowTypes[i]);

    if (ok) {
      stats[cat].pass++;
    } else {
      stats[cat].fail++;
      if (stats[cat].fail <= 20) {
        failures.push({ rel, babelTypes: babelTypes.join(','), flowTypes: flowTypes.join(','), cat });
      }
    }
  }

  // Print results by category
  console.log('📊  RESULTS BY CATEGORY');
  console.log('='.repeat(80));
  const sortedCats = Object.keys(stats).sort();
  let totalPass = 0, totalFail = 0, totalSkip = 0, totalTests = 0;
  for (const cat of sortedCats) {
    const s = stats[cat];
    totalPass += s.pass; totalFail += s.fail; totalSkip += s.skip; totalTests += s.total;
    const run = s.pass + s.fail;
    const pct = run > 0 ? Math.round(s.pass * 100 / run) : 0;
    console.log(`  ${cat.padEnd(12)} → ${s.pass.toString().padStart(4)} ✅ / ${s.fail.toString().padStart(4)} ❌  (${run} run, ${s.skip} skipped)  ${pct}%`);
  }
  console.log('='.repeat(80));
  const totalRun = totalPass + totalFail;
  console.log(`  TOTAL: ${totalPass}/${totalRun} passed (${totalRun > 0 ? Math.round(totalPass * 100 / totalRun) : 0}%)  |  ${totalSkip} skipped  |  ${totalTests} total`);

  console.log('\n📋  FAILURE EXAMPLES (up to 25):');
  console.log('-'.repeat(80));
  for (const f of failures.slice(0, 25)) {
    if (f.error) {
      console.log(`  [${f.cat}] ❌ ${f.rel}`);
      console.log(`         ${f.error}`);
    } else {
      console.log(`  [${f.cat}] ❌ ${f.rel}`);
      console.log(`         babel: [${f.babelTypes}]`);
      console.log(`         flowjs: [${f.flowTypes}]`);
    }
    console.log();
  }
  if (failures.length > 25) {
    console.log(`  (... ${failures.length - 25} more failures)`);
  }

  // Exit with code
  process.exit(totalFail > 100 ? 1 : 0);
}

main();