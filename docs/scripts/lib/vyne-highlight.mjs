// Minimal syntax highlighter for Vyne snippets, emitting the same token
// classes used across the documentation.
//
// Output is safe HTML. Unknown constructs fall through as escaped text.

const KEYWORDS = new Set([
  "fn", "interface", "group", "module", "enum", "const", "region", "use",
  "deploy", "dismiss", "lib", "as", "ruleset", "scratch", "native", "external",
  "if", "else", "while", "through", "loop", "collect", "filter", "every",
  "unique", "break", "continue", "return", "in",
  "try", "catch", "finally", "throw", "defer", "drop",
  "requires", "ensures", "result", "backward", "grad",
  "warnings", "dynamic_casting", "memory_limit", "type_check",
  "implicit_casting", "warnings_ignore", "strict", "pedantic", "verbose",
  "profiling", "debug", "trace", "contracts",
  "true", "false", "null",
  "self", "_err", "_",
]);

const OPERATORS = [
  "??=", "|>", "??", "++", "--", ">=", "<=", "==", "!=", "&&", "||",
  "::", ":=", "->", "..", "=>", "**", "//", "<<", ">>", "^^",
  "+", "-", "*", "/", "%", "=", "<", ">", "?", "!", "&", "|", "^", "~", "$",
].sort((a, b) => b.length - a.length);

const TOKEN_RE = new RegExp(
  [
    "(\\/\\/[^\\n]*|#[^\\n]*)", // 1 comment
    '("(?:[^"\\\\]|\\\\.)*")', // 2 string
    "(\\b0[xX][0-9A-Fa-f_]+\\b|\\b\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d+)?[fF]?\\b)", // 3 number
    "([A-Za-z_][A-Za-z0-9_]*)", // 4 identifier
    `(${OPERATORS.map((o) => o.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|")})`, // 5 operator
    "(\\s+)", // 6 whitespace
    "([\\s\\S])", // 7 any other char
  ].join("|"),
  "g",
);

function escapeHtml(s) {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

const wrap = (cls, text) => `<span class="${cls}">${escapeHtml(text)}</span>`;

export function highlightVyne(code) {
  const tokens = [];
  let m;
  TOKEN_RE.lastIndex = 0;
  while ((m = TOKEN_RE.exec(code)) !== null) {
    if (m[0] === "") {
      TOKEN_RE.lastIndex++;
      continue;
    }
    tokens.push({
      kind: m[1] ? "comment"
        : m[2] ? "string"
        : m[3] ? "number"
        : m[4] ? "ident"
        : m[5] ? "op"
        : m[6] ? "space"
        : "other",
      value: m[0],
    });
  }

  let out = "";
  for (let i = 0; i < tokens.length; i++) {
    const tk = tokens[i];
    if (tk.kind === "comment") out += wrap("c-com", tk.value);
    else if (tk.kind === "string") out += wrap("c-str", tk.value);
    else if (tk.kind === "number") out += wrap("c-num", tk.value);
    else if (tk.kind === "op") out += wrap("c-op", tk.value);
    else if (tk.kind === "ident") {
      if (KEYWORDS.has(tk.value)) out += wrap("c-kw", tk.value);
      else {
        // look ahead for a call
        let j = i + 1;
        while (j < tokens.length && tokens[j].kind === "space") j++;
        if (tokens[j] && tokens[j].value === "(") out += wrap("c-fn", tk.value);
        else if (/^[A-Z]/.test(tk.value)) out += wrap("c-ty", tk.value);
        else out += escapeHtml(tk.value);
      }
    } else out += escapeHtml(tk.value);
  }
  return out;
}
