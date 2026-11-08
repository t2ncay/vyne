# vcsv/Types.vy - flat CSV table.
#
# `cells` is row-major and flat: cell(r, c) is cells[r * n_cols + c].
# Headers are stored separately; `headers` is empty when the file was
# parsed without a header row. Both arrays are boxed string arrays, which
# keeps the parser simple and the interface C-eligible.

ruleset { dynamic_casting };

module vcsv;

interface Table {
    headers :: Array,
    cells   :: Array,
    n_rows  :: Int64,
    n_cols  :: Int64,
}
