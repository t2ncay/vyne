# Contributing to {{name}}

Thanks for taking a look.

## Development loop

```bash
vynec run src/main.vy
vynec run examples/01_hello.vy
```

## Before opening a pull request

- Keep the demo runnable with `vynec run src/main.vy`.
- Match the style in `src/`: typed declarations use `:=`, reassignment
  uses `=`.
- If you change `data/iris.data`, update the `[data].iris.sha256` entry
  in `project.toml`.

## License

{{#if license_spdx}}
This project is licensed under {{license_spdx}}. See `LICENSE`.
{{/if}}
{{#if proprietary}}
This project is proprietary. Replace `LICENSE_HEADER.txt` with your
own terms before distributing it.
{{/if}}
