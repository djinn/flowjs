#!/usr/bin/env node
/* flowjs CLI — babel-like transpiler command-line interface */
'use strict';

const path = require('path');
const fs = require('fs');

// Load native module
let flow;
try {
  flow = require(path.join(__dirname, '..', 'build', 'Release', 'flowjs_native'));
} catch (e) {
  console.error('Error: flowjs native module not found. Run `npm run build` first.');
  process.exit(1);
}

function main() {
  const args = process.argv.slice(2);

  if (args.length === 0 || args.includes('--help') || args.includes('-h')) {
    console.log(`
flowjs v${flow.version}
Usage: flowjs <file.js> [options]

Options:
  -o, --out-file <file>  Write output to file (default: stdout)
  --ast                  Output AST JSON instead of transformed source
  --help, -h             Show this help

Examples:
  flowjs input.js                 # parse and output AST to stdout
  flowjs input.js --ast           # same, explicitly
  flowjs input.js -o output.js    # parse and write to file
`);
    return;
  }

  let outFile = null;
  let showAst = false;
  const files = [];

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '-o' || arg === '--out-file') {
      outFile = args[++i];
    } else if (arg === '--ast') {
      showAst = true;
    } else if (arg.startsWith('-')) {
      console.error(`Unknown option: ${arg}`);
      process.exit(1);
    } else {
      files.push(arg);
    }
  }

  if (files.length === 0) {
    console.error('No input files specified. Use --help for usage.');
    process.exit(1);
  }

  let allOutput = '';
  for (const file of files) {
    const resolved = path.resolve(file);
    if (!fs.existsSync(resolved)) {
      console.error(`File not found: ${file}`);
      process.exit(1);
    }
    try {
      const result = flow.transformFile(resolved);
      allOutput += (files.length > 1 ? `// ${path.basename(file)}\n` : '') + result + '\n';
    } catch (err) {
      console.error(`Error processing ${file}: ${err.message}`);
      process.exit(1);
    }
  }

  if (outFile) {
    fs.writeFileSync(outFile, allOutput, 'utf-8');
    console.log(`Written to ${outFile}`);
  } else {
    process.stdout.write(allOutput);
  }
}

if (require.main === module) {
  main();
}

module.exports = { flow };