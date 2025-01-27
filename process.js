const fs = require('fs');
const os = require('os');
const path = require('path');
const readline = require('readline');

const RGRAPH = /^Running on \s*.*\/(.*?)\.mtx\.edges \.\.\./m;
const RORDER = /^Nodes: (\d+), Edges: (\d+)/m;
const RBATCH = /^Batch fraction: (.+?) \[(.+?) edges\]/m;
const RTRANS = /^Transposing graph \.\.\./m;
const RDELET = /^Deleting edges \[(.+?) edges\] \.\.\./m;
const RINSRT = /^Inserting edges \[(.+?) edges\] \.\.\./m;
const RVISIT = /^Testing multi-step visit count with BFS \.\.\./m;
const RETIME = /^Elapsed time: (.+?) ms/m;




// *-FILE
// ------

function readFile(pth) {
  var d = fs.readFileSync(pth, 'utf8');
  return d.replace(/\r?\n/g, '\n');
}

function writeFile(pth, d) {
  d = d.replace(/\r?\n/g, os.EOL);
  fs.writeFileSync(pth, d);
}




// *-CSV
// -----

function writeCsv(pth, rows) {
  var cols = Object.keys(rows[0]);
  var a = cols.join()+'\n';
  for (var r of rows)
    a += [...Object.values(r)].map(v => `"${v}"`).join()+'\n';
  writeFile(pth, a);
}




// *-LOG
// -----

function readLogLine(ln, data, state) {
  state = state || {};
  ln = ln.replace(/^\d+-\d+-\d+ \d+:\d+:\d+\s+/, '');
  if (RGRAPH.test(ln)) {
    var [, graph] = RGRAPH.exec(ln);
    if (!data.has(graph)) data.set(graph, []);
    state.graph = graph;
    state.order = 0;
    state.size  = 0;
    state.batch_fraction = 0;
    state.batch_edges    = 0;
    state.time      = 0;
    state.technique = 'readGraph';
  }
  else if (RORDER.test(ln)) {
    var [, order, size] = RORDER.exec(ln);
    state.order = parseFloat(order);
    state.size  = parseFloat(size);
  }
  else if (RBATCH.test(ln)) {
    var [, batch_fraction, batch_edges] = RBATCH.exec(ln);
    state.batch_fraction = parseFloat(batch_fraction);
    state.batch_edges    = parseFloat(batch_edges);
  }
  else if (RTRANS.test(ln)) {
    state.technique = 'transposeGraph';
  }
  else if (RDELET.test(ln)) {
    state.technique = 'deleteEdges';
  }
  else if (RINSRT.test(ln)) {
    state.technique = 'insertEdges';
  }
  else if (RVISIT.test(ln)) {
    var isInsert = state.technique==='insertEdges';
    var isVisitP = state.technique==='visitGraph+';
    state.technique = isInsert || isVisitP? 'visitGraph+' : 'visitGraph-';
  }
  else if (RETIME.test(ln)) {
    var [, time] = RETIME.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      time: parseFloat(time),
    }));
  }
  return state;
}

function readLog(pth) {
  var text  = readFile(pth);
  var lines = text.split('\n');
  var data  = new Map();
  var state = null;
  for (var ln of lines)
    state = readLogLine(ln, data, state);
  return data;
}




// PROCESS-*
// ---------

function processCsv(data) {
  var a = [];
  for (var rows of data.values())
    a.push(...rows);
  return a;
}




// HEADER LINES
// ------------

// Count the number of header lines in a MatrixMarket file.
async function headerLines(pth) {
  var a  = 0;
  var rl = readline.createInterface({input: fs.createReadStream(pth)});
  for await (var line of rl) {
    if (line[0]==='%') ++a;
    else break;
  }
  return a+1;  // +1 for the row/column count line
}




// MAIN
// ----

async function main(cmd, inp, out) {
  var data = cmd==='csv'? readLog(inp) : '';
  if (out && path.extname(out)==='') cmd += '-dir';
  switch (cmd) {
    case 'csv':
      var rows = processCsv(data);
      writeCsv(out, rows);
      break;
    case 'csv-dir':
      for (var [graph, rows] of data)
        writeCsv(path.join(out, graph+'.csv'), rows);
      break;
    case 'header-lines':
      var lines = await headerLines(inp);
      console.log(lines);
      break;
    default:
      console.error(`error: "${cmd}"?`);
      break;
  }
}
main(...process.argv.slice(2));
