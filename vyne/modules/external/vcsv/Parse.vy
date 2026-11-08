# vcsv/Parse.vy - RFC 4180-style parser.
#
# Supports quoted fields, doubled quotes inside quotes, embedded commas
# and newlines, CRLF input, and a caller-supplied single-character
# delimiter. Fields are accumulated with the vcore string builder.

use "Types.vy";
use native vcore;
use native vfs;

ruleset { dynamic_casting };

module vcsv;

fn :: vcsv parse_with(text :: String, delim :: String,
                       has_header :: Bool) -> vcsv.Table {
    if delim.length() == 0 {
        throw "vcsv.parse_with: delimiter must be one character";
    }
    dcode :: Int64 := delim.byte_at(0);
    n :: Int64 := text.length();
    cells :: Array := [];
    sb :: Int64 := vcore.sb_create();
    in_quotes :: Bool := false;
    row_cells :: Int64 := 0;
    n_cols :: Int64 := 0;
    first_row :: Bool := true;
    row_pending :: Bool := false;
    i :: Int64 := 0;

    while i < n {
        code :: Int64 := text.byte_at(i);
        if in_quotes {
            if code == 34 {
                if i + 1 < n && text.byte_at(i+1) == 34 {
                    vcore.sb_append_char(sb, 34);
                    i = i + 2;
                } else {
                    in_quotes = false;
                    i = i + 1;
                }
            } else {
                vcore.sb_append_char(sb, code);
                i = i + 1;
            }
        } else {
            if code == 34 {
                in_quotes = true;
                row_pending = true;
                i = i + 1;
            } else if code == dcode {
                cells.push(vcore.sb_build(sb));
                sb = vcore.sb_create();
                row_cells = row_cells + 1;
                row_pending = true;
                i = i + 1;
            } else if code == 10 {
                cells.push(vcore.sb_build(sb));
                sb = vcore.sb_create();
                row_cells = row_cells + 1;
                if first_row {
                    n_cols = row_cells;
                    first_row = false;
                } else if row_cells != n_cols {
                    throw "vcsv.parse: inconsistent column count";
                }
                row_cells = 0;
                row_pending = false;
                i = i + 1;
            } else if code == 13 {
                i = i + 1;
            } else {
                vcore.sb_append_char(sb, code);
                row_pending = true;
                i = i + 1;
            }
        }
    }

    if in_quotes { throw "vcsv.parse: unclosed quoted field"; }

    if row_pending {
        cells.push(vcore.sb_build(sb));
        row_cells = row_cells + 1;
        if first_row {
            n_cols = row_cells;
            first_row = false;
        } else if row_cells != n_cols {
            throw "vcsv.parse: inconsistent column count";
        }
    }

    if n_cols <= 0 {
        return vcsv.Table([], [], 0, 0);
    }

    headers :: Array := [];
    start :: Int64 := 0;
    if has_header {
        through c :: 0..n_cols-1 -> loop { headers.push(cells[c]); };
        start = n_cols;
    }

    data_cells :: Array := [];
    if start < cells.size() {
        through i2 :: start..cells.size()-1 -> loop {
            data_cells.push(cells[i2]);
        };
    }
    n_rows :: Int64 := data_cells.size() / n_cols;
    return vcsv.Table(headers, data_cells, n_rows, n_cols);
}

fn :: vcsv parse(text :: String, has_header :: Bool) -> vcsv.Table {
    return vcsv.parse_with(text, ",", has_header);
}

fn :: vcsv parse_file_with(path :: String, delim :: String,
                           has_header :: Bool) -> vcsv.Table {
    text :: String := vfs.read(path);
    return vcsv.parse_with(text, delim, has_header);
}

fn :: vcsv parse_file(path :: String, has_header :: Bool) -> vcsv.Table {
    return vcsv.parse_file_with(path, ",", has_header);
}
