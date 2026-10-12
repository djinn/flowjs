// flowjs example — modern JS features to parse
let x = 42;
const y = "hello";
var z = x + y;

function add(a, b) {
  return a + b;
}

// Template literal
const name = "world";
const msg = `Hello, ${name}!`;

// Arrow function
const square = (n) => n * n;

// Optional chaining
const val = obj?.prop?.nested;

// Destructuring
const [a, b] = [1, 2];
const { width, height } = { width: 100, height: 200 };

// Spread in object
const base = { a: 1 };
const merged = { ...base, b: 2 };

// Method shorthand
const obj = {
  greet() {
    return `Hi, ${this.name}`;
  }
};

// Computed property
const key = "dynamic";
const dynamic = { [key]: "value" };

// Class with inheritance
class Animal {
  constructor(name) {
    this.name = name;
  }
  speak() {
    return `${this.name} makes a sound`;
  }
}

class Dog extends Animal {
  speak() {
    return `${this.name} barks`;
  }
}

// Async function
async function fetchData() {
  return await fetch("/api/data");
}

// Labeled loop
outer: for (let i = 0; i < 10; i++) {
  inner: for (let j = 0; j < 10; j++) {
    if (j > i) break outer;
    if (j === i) continue inner;
  }
}

// Generator
function* fibonacci() {
  let [a, b] = [0, 1];
  while (true) {
    yield a;
    [a, b] = [b, a + b];
  }
}