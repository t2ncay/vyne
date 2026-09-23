# Vyne for Visual Studio Code

> Syntax highlighting, bracket matching, and language support for the **Vyne** programming language.

[![Version](https://img.shields.io/badge/version-0.3.0-blue.svg)](https://github.com/t2ncay/vyne)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![VS Code](https://img.shields.io/badge/VS%20Code-%5E1.75.0-007ACC.svg)](https://code.visualstudio.com/)

---

## ✨ Features

- **Full syntax highlighting** for every Vyne construct
- **Generic type highlighting** — `Box<T>`, `Pair<A, B>`, nested `Array<Box<T>>`
- **String interpolation** with embedded expression highlighting
- **Type annotation scopes** for `::` (distinct from other operators)
- **Module injection syntax** — `fn :: MathUtils square(...)`
- **Bracket matching, auto-closing, and smart indentation**
- **Code folding** with `# region` / `# endregion` markers
- **Semantic-color-aware** — uses standard TextMate scopes so it inherits your theme's palette

---

## 📦 Installation

### From the Marketplace (once published)

1. Open VS Code.
2. Press `Ctrl+P` (or `Cmd+P` on macOS).
3. Paste: `ext install tuncay-gafarli.vyne-language`
4. Press Enter.

### From a `.vsix` file

1. Download `vyne-language-0.1.0.vsix`.
2. In VS Code, open the Command Palette (`Ctrl+Shift+P`).
3. Run **`Extensions: Install from VSIX...`**.
4. Select the file.

Or from the terminal:

```bash
code --install-extension vyne-language-0.1.0.vsix
```

````

### From source

```bash
git clone https://github.com/your-username/vyne-vscode
cd vyne-vscode
npm install -g @vscode/vsce
vsce package
code --install-extension vyne-language-0.1.0.vsix
````

---

## 🎨 Recommended Themes

The grammar uses **standard TextMate scopes**, so it inherits colors from any theme
that supports semantic highlighting. Best results with:

| Theme               | Notes                                |
| ------------------- | ------------------------------------ |
| **Dark+ (default)** | Full support for all Vyne scopes     |
| **One Dark Pro**    | Rich operator and keyword coloring   |
| **Tokyo Night**     | Excellent type / function separation |
| **Catppuccin**      | Soft palette, great contrast         |
| **Nord**            | Cool tones, very readable            |
| **Dracula**         | High contrast keywords               |
| **GitHub Dark**     | Familiar to most developers          |

---

## 🔍 Scope Reference

Every Vyne construct maps to a standard TextMate scope. This table is useful
if you want to write a custom theme or override specific colors.

### Declarations

| Vyne construct | TextMate scope                | Typical color    |
| -------------- | ----------------------------- | ---------------- |
| `fn`           | `storage.type.function.vyne`  | purple / magenta |
| `interface`    | `storage.type.interface.vyne` | purple / magenta |
| `group`        | `storage.type.group.vyne`     | purple / magenta |
| `enum`         | `storage.type.enum.vyne`      | purple / magenta |
| `module`       | `storage.type.module.vyne`    | purple / magenta |
| `ruleset`      | `storage.type.ruleset.vyne`   | purple / magenta |
| `const`        | `storage.modifier.const.vyne` | blue             |
| `defer`        | `storage.modifier.defer.vyne` | blue             |

### Identifiers

| Vyne construct              | TextMate scope                        | Typical color |
| --------------------------- | ------------------------------------- | ------------- |
| Function name (declaration) | `entity.name.function.vyne`           | yellow / gold |
| Function call               | `entity.name.function.call.vyne`      | yellow        |
| Interface / type name       | `entity.name.type.interface.vyne`     | teal / cyan   |
| Generic type parameter      | `entity.name.type.generic-param.vyne` | teal italic   |
| Module namespace            | `entity.name.namespace.vyne`          | teal          |
| Parameter name              | `variable.parameter.vyne`             | orange / red  |
| Local variable              | `variable.other.vyne`                 | light blue    |
| Member (`x.field`)          | `variable.other.member.vyne`          | light blue    |
| `self`                      | `variable.language.self.vyne`         | italic blue   |

### Literals

| Vyne construct   | TextMate scope                   | Typical color             |
| ---------------- | -------------------------------- | ------------------------- |
| `true` / `false` | `constant.language.boolean.vyne` | blue                      |
| `null`           | `constant.language.null.vyne`    | blue                      |
| `42`             | `constant.numeric.integer.vyne`  | green                     |
| `3.14`           | `constant.numeric.float.vyne`    | green                     |
| `"text"`         | `string.quoted.double.vyne`      | orange / green            |
| `"{expr}"`       | `meta.embedded.expression.vyne`  | (recursively highlighted) |
| `\n` `\t` `\e`   | `constant.character.escape.vyne` | cyan                      |

### Keywords

| Vyne construct                             | TextMate scope                          | Typical color |
| ------------------------------------------ | --------------------------------------- | ------------- |
| `if` `else` `while` `through`              | `keyword.control.vyne`                  | purple / pink |
| `return` `break` `continue`                | `keyword.control.vyne`                  | purple / pink |
| `loop` `collect` `every` `filter` `unique` | `keyword.control.loop-mode.vyne`        | purple / pink |
| `try` `catch` `finally` `throw`            | `keyword.control.exception.vyne`        | purple / red  |
| `use` `deploy` `dismiss`                   | `keyword.control.module-lifecycle.vyne` | purple        |
| `in`                                       | `keyword.control.vyne`                  | purple        |

### Types

| Vyne construct                                  | TextMate scope                          | Typical color |
| ----------------------------------------------- | --------------------------------------- | ------------- |
| `Int64` `Float64` `String` `Array` `Bool` `Map` | `support.type.primitive.vyne`           | teal          |
| Custom type (`Box`, `Point`)                    | `entity.name.type.vyne`                 | teal          |
| `::` before type                                | `keyword.operator.type-annotation.vyne` | cyan          |
| `::` in `fn :: module`                          | `keyword.operator.injection.vyne`       | cyan          |
| `->` return arrow                               | `keyword.operator.arrow.vyne`           | cyan          |
| `<...>` generic args                            | `punctuation.definition.generic.*.vyne` | grey          |

### Operators

| Vyne construct              | TextMate scope                        | Typical color |
| --------------------------- | ------------------------------------- | ------------- |
| `+` `-` `*` `/` `%`         | `keyword.operator.arithmetic.vyne`    | cyan          |
| `==` `!=` `<` `>` `<=` `>=` | `keyword.operator.logical.vyne`       | cyan          |
| `&&` `\|\|`                 | `keyword.operator.logical.vyne`       | cyan          |
| `=`                         | `keyword.operator.assignment.vyne`    | cyan          |
| `\|>` pipeline              | `keyword.operator.pipeline.vyne`      | cyan bold     |
| `??` `??=` null-coalesce    | `keyword.operator.null-coalesce.vyne` | cyan          |
| `..` range                  | `keyword.operator.range.vyne`         | cyan          |
| `&` `$` reference           | `keyword.operator.reference.vyne`     | cyan          |

### Built-ins

| Vyne construct                                   | TextMate scope                         | Typical color |
| ------------------------------------------------ | -------------------------------------- | ------------- |
| `out` `sizeof` `type` `string` `int64` `float64` | `support.function.builtin.vyne`        | blue          |
| `vcore` `vglib` `vmath` ...                      | `support.constant.builtin-module.vyne` | blue          |

---

## 🧪 Try It

Open any `.vy` file. To verify all scopes are working, save this as `test.vy`:

```vyne
# Vyne syntax highlight test
ruleset { dynamic_casting, verbose }

use lib "vcolors.vy";

interface Box<T> {
    value :: T;
}

interface Pair<A, B> {
    first  :: A;
    second :: B;
}

group MathUtils {
    PI = 3.14159;
}

enum Status {
    Ok, Error, Pending
}

fn identity<T>(x :: T) -> T {
    return x;
}

fn :: MathUtils square(n :: Int64) -> Int64 {
    return n * n;
}

fn main() {
    b :: Box<Int64> = Box<Int64>(42);
    p :: Pair<Int64, String> = Pair<Int64, String>(7, "seven");

    name :: String = "World";
    greeting = "Hello, {name}!";

    out(b.value);
    out(p.first);
    out(greeting);

    x :: Int64 = 10;
    y = identity<Int64>(x);
    z = x ?? 0;

    result = [1, 2, 3] |> MathUtils.square;

    try {
        throw "error";
    } catch (e) {
        out(e);
    } finally {
        out("done");
    }

    defer {
        out("cleanup");
    }
}

main();
```

Then use `Ctrl+Shift+P` → **Developer: Inspect Editor Tokens and Scopes** and click
any token to confirm it's highlighting correctly.

---

## 🛠 Development

The extension is a pure TextMate grammar — **no language server, no runtime
dependency**. Edits to `syntaxes/vyne.tmLanguage.json` take effect immediately.

### Live iteration

1. Open the `vyne-vscode/` folder in VS Code.
2. Press `F5`. A new **Extension Development Host** window opens.
3. Open any `.vy` file in that window.
4. Edit the grammar in the main window.
5. Press `Ctrl+R` in the Extension Development Host to reload.

### Debugging a token

If a token colors wrong:

1. `Ctrl+Shift+P` → **Developer: Inspect Editor Tokens and Scopes**.
2. Click the token.
3. The panel shows:
   - **Language**: `vyne`
   - **TextMate scope**: the deepest matched scope
   - **All scopes**: the full stack, from most general to most specific
4. Match the deepest scope against `syntaxes/vyne.tmLanguage.json` to find
   which pattern matched, then adjust it.

### Packaging

```bash
vsce package
```

Produces `vyne-language-0.1.0.vsix` in the project root.

---

## 📁 Project Structure

```
vyne-vscode/
├── package.json                  # Extension manifest
├── language-configuration.json   # Brackets, comments, indentation
├── syntaxes/
│   └── vyne.tmLanguage.json      # The grammar itself
├── .vscodeignore                 # Files excluded from the package
├── README.md                     # This file
├── CHANGELOG.md                  # Version history
├── LICENSE                       # MIT
└── icon.png                      # (optional) 128×128 extension icon
```

---

## 🎨 Customizing Colors

If your theme doesn't color a Vyne-specific scope the way you want, override it
in your `settings.json`:

```json
{
  "editor.tokenColorCustomizations": {
    "textMateRules": [
      {
        "scope": "keyword.operator.pipeline.vyne",
        "settings": { "foreground": "#FF79C6", "fontStyle": "bold" }
      },
      {
        "scope": "keyword.operator.type-annotation.vyne",
        "settings": { "foreground": "#8BE9FD" }
      },
      {
        "scope": "entity.name.type.generic-param.vyne",
        "settings": { "foreground": "#50FA7B", "fontStyle": "italic" }
      },
      {
        "scope": "variable.parameter.vyne",
        "settings": { "foreground": "#FFB86C" }
      }
    ]
  }
}
```

The scopes above are the ones most themes don't have built-in rules for.
Adding them gives you complete control over the Vyne look.

---

## 🐛 Known Limitations

- **TextMate is regex-based**, not a real parser. Extremely contrived code
  (e.g. `a<b` where `a` is capitalized and the line ends in `(`) may highlight
  slightly differently than the actual Vyne parser would parse it.
- **No semantic tokens yet.** Parameter vs. local variable distinction relies on
  naming convention (`variable.parameter` matches parameters because they
  appear inside `fn (...)` — but a local named `x` at the top level of a function
  also matches `variable.other.vyne`).
- **Cross-file type resolution is not possible in a TextMate grammar.** If you
  write `x :: MyStruct` and `MyStruct` is defined in another file, the grammar
  still colors it correctly (PascalCase → `entity.name.type.vyne`) without
  needing to know where `MyStruct` lives.

---

## 🤝 Contributing

Issues and pull requests are welcome.

When reporting a syntax highlighting bug:

1. Include the **smallest** `.vy` snippet that reproduces the issue.
2. Include the output of **Developer: Inspect Editor Tokens and Scopes**.
3. Note which theme you're using.

---

## 📜 License

MIT © 2026 Tuncay Gafarli

---

## 🔗 Related

- **Vyne compiler** — [github.com/your-username/vyne](https://github.com/your-username/vyne)
- **Vyne runtime** — included in the compiler repo under `runtime/`
- **Vyne external modules** — `vcolors`, and others shipped under `modules/external/`

---

**Made with ❤️ for the Vyne programming language.**
