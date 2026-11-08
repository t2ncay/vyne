# vcsv — CSV tables for Vyne

`vcsv` parses and writes RFC 4180-style CSV (and TSV) with quoted fields,
doubled quotes, embedded delimiters/newlines, CRLF input, and an optional header
row. It is designed for data ingestion into `vstat`, `vinterp`, `vlin`, and the
plotting stack.

```vyne
use external "vcsv/vcsv.vy";
use native vfs;

text :: String := vfs.read("data.csv");
t :: vcsv.Table := vcsv.parse(text, true);

out(vcsv.header(t, 0));            # first header
out(vcsv.cell(t, 0, 0));           # first data cell
xs :: Array<Float64> := vcsv.column_f64(t, 0);
ys :: Array<Float64> := vcsv.column_f64(t, 1);
```

## Table

A table is stored flat and row-major:

```vyne
interface Table {
    headers :: Array,   # empty when parsed without a header
    cells   :: Array,   # n_rows * n_cols strings, row-major
    n_rows  :: Int64,
    n_cols  :: Int64,
}
```

`cell(t, r, c)` is `cells[r * n_cols + c]`. The flat layout keeps the parser and
serializer simple and lets typed-column extraction stream over the cells.

## Parsing

| Function | Result |
| --- | --- |
| `parse(text, has_header)` | comma-separated table |
| `parse_with(text, delim, has_header)` | custom single-character delimiter |
| `parse_file(path, has_header)` | read via `vfs` and parse commas |
| `parse_file_with(path, delim, has_header)` | read via `vfs` and parse a delimiter |

Rules:

- Fields may be quoted with `"`.
- Inside a quoted field, `""` is one literal `"`; delimiters and newlines are
  literal.
- A quoted field must be closed; an unclosed quote throws.
- Every row must have the same number of columns as the first row.
- `\r\n` is accepted; `\r` outside quotes is skipped.
- With `has_header = true`, the first row is stored in `headers` and is not part
  of `cells`. Otherwise `headers` is empty and every row is data.

## Access

| Function | Result |
| --- | --- |
| `cell(t, r, c)` | string |
| `header(t, c)` | header string (throws if no headers) |
| `has_headers(t)` | `Bool` |
| `shape(t)` | `[n_rows, n_cols]` |
| `row(t, r)` | boxed array of cell strings |
| `column(t, c)` | boxed array of cell strings |
| `column_f64(t, c)` | `Array<Float64>` (parsed per cell) |
| `column_i64(t, c)` | `Array<Int64>` (parsed per cell) |
| `column_names(t)` | the headers array |
| `select_columns(t, indices)` | a new table with the chosen columns |

`column_f64`/`column_i64` use the language's numeric conversions; malformed
cells convert to zero. Validate before converting when the data is untrusted.

## Writing

| Function | Result |
| --- | --- |
| `needs_quote(s)` | true if the field needs quoting |
| `quote_field(s)` | quoted and escaped field |
| `serialize(t)` | comma-separated string with a trailing newline |
| `serialize_with(t, delim)` | custom delimiter |
| `write_file(t, path)` | write via `vfs` |
| `write_file_with(t, path, delim)` | write with a custom delimiter |

Fields containing the delimiter, a quote, `\n`, or `\r` are quoted and their
quotes are doubled. The serializer does not preserve original key/row order
beyond the table's row order.

## Tests and benchmark

```bash
./vynec --compile examples/test/vcsv_test.vy
./examples/test/vcsv_test

./vynec --compile examples/benchmark/vcsv_bench.vy
./examples/benchmark/vcsv_bench
```

The cross-library integration test also exercises `vcsv` alongside `vstat`,
`vinterp`, `vcomplex`, and `vtermplot`:

```bash
./vynec --compile examples/test/libraries_integration_test.vy
./examples/test/libraries_integration_test
```
