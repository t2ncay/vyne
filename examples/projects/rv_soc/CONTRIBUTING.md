# Contributing to rv_soc

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


This project is licensed under MIT. See `LICENSE`.


