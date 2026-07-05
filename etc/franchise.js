// anime-title-generator.js
// node anime-title-generator.js

const emotional = [
  "Silent", "Hollow", "Scarlet", "Crimson", "Infinite", "Lost",
  "Forgotten", "Broken", "Artificial", "Eternal", "Divine",
  "Phantom", "Empty", "Final", "Frozen", "Velvet", "Iron",
  "Black", "White", "Azure", "Astral", "Lunar", "Guilty",
  "False", "Last", "Dead", "Mechanical", "Sleeping", "Zero"
];

const technical = [
  "Protocol", "Theory", "Works", "System", "Code", "Formula",
  "Archive", "Engine", "Circuit", "Project", "Sequence",
  "Index", "Signal", "Record", "Framework", "Interface",
  "Pattern", "Memory", "Frequency", "Algorithm", "Directive",
  "Vector", "Terminal", "Module", "Operator"
];

const object = [
  "Blade", "Moon", "Garden", "Heaven", "Cathedral", "Paradise",
  "Kingdom", "Mirror", "Dream", "Genesis", "Horizon", "Ash",
  "Prayer", "Symphony", "Crown", "Bloom", "Night", "Oracle",
  "Children", "Sky", "World", "Star", "Machine", "Requiem",
  "Mirage", "Oblivion", "Archive", "Eclipse"
];

const connectors = [
  "of", "for", "Beyond", "Under", "Against", "Without",
  "Before", "After", "Between", "Inside"
];

const greek = [
  "Alpha", "Beta", "Gamma", "Delta", "Omega", "Sigma"
];

const symbols = [
  "/",
  ":",
  ";",
  "-",
  "×",
];

const patterns = [
  () => `${e()} ${t()} ${o()}`,
  () => `${e()} ${o()} ${t()}`,
  () => `${o()} ${t()} ${e()}`,
  () => `${e()} ${o()} ${pick(connectors)} ${o()}`,
  () => `${t()} ${o()} ${pick(connectors)} ${o()}`,
  () => `${e()} ${pick(symbols)} ${o()}`,
  () => `${e()}/${o()}`,
  () => `${e()};${o()}`,
  () => `${o()}:${t()}`,
  () => `${e()} ${t()}:${o()}`,
  () => `${o()} ${pick(greek)}`,
  () => `${e()} ${o()} ${pick(greek)}`,
  () => `${t()}-${o()}`,
  () => `${e()} ${o()} Works`,
  () => `${e()} ${o()} Theory`,
];

function pick(arr) {
  return arr[Math.floor(Math.random() * arr.length)];
}

const e = () => pick(emotional);
const t = () => pick(technical);
const o = () => pick(object);

function postProcess(title) {
  // Occasionally lowercase connector words
  title = title.replace(/\b(Of|For|Beyond|Under|Against|Without|Before|After|Between|Inside)\b/g,
    m => Math.random() < 0.7 ? m.toLowerCase() : m);

  // Rarely replace "and" with "&" if it exists
  title = title.replace(/\band\b/g, "&");

  // Rarely prepend a version/code
  if (Math.random() < 0.08) {
    title = `${pick(["Type", "Code", "Project", "Phase"])}-${pick(greek)} ${title}`;
  }

  // Rarely append a weird suffix
  if (Math.random() < 0.12) {
    title += pick([
      " Zero",
      " ∞",
      " EX",
      " ver.β",
      " //",
      " .hack",
      " 01"
    ]);
  }

  return title;
}

const results = new Set();

while (results.size < 20) {
  results.add(postProcess(pick(patterns)()));
}

console.log([...results].join("\n"));