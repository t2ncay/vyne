# vcsv_test.vy - parsing, quoting, typed columns, and file round-trip.

use external "vcsv/vcsv.vy";
use native vfs;

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vcsv ===");

text :: String := "name,age,score\n\"Ada, Lovelace\",36,99.5\n\"Grace \"\"Amazing\"\" Hopper\",85,88.0\nAlan,41,91.0\n";
t :: vcsv.Table := vcsv.parse(text, true);

check("n_rows", t.n_rows == 3);
check("n_cols", t.n_cols == 3);
check("has_headers", vcsv.has_headers(t));
check("header-0", vcsv.header(t, 0) == "name");
check("header-2", vcsv.header(t, 2) == "score");
check("quoted-comma", vcsv.cell(t, 0, 0) == "Ada, Lovelace");
check("doubled-quote", vcsv.cell(t, 1, 0) == "Grace \"Amazing\" Hopper");
check("plain-cell", vcsv.cell(t, 2, 0) == "Alan");

scores :: Array<Float64> := vcsv.column_f64(t, 2);
check("column_f64-len", scores.size() == 3);
check("column_f64-0", close(scores[0], 99.5));
check("column_f64-1", close(scores[1], 88.0));
check("column_f64-2", close(scores[2], 91.0));

ages :: Array<Int64> := vcsv.column_i64(t, 1);
check("column_i64-0", ages[0] == 36);
check("column_i64-1", ages[1] == 85);
check("column_i64-2", ages[2] == 41);

sh :: Array := vcsv.shape(t);
check("shape-rows", int64(sh[0]) == 3);
check("shape-cols", int64(sh[1]) == 3);

r0 :: Array := vcsv.row(t, 0);
check("row-len", r0.size() == 3);
check("row-cell", r0[0] == "Ada, Lovelace");
col0 :: Array := vcsv.column(t, 0);
check("column-len", col0.size() == 3);
check("column-cell", col0[1] == "Grace \"Amazing\" Hopper");

sel :: vcsv.Table := vcsv.select_columns(t, [0, 2]);
check("select-cols", sel.n_cols == 2);
check("select-header", vcsv.header(sel, 1) == "score");
check("select-cell", vcsv.cell(sel, 0, 1) == "99.5");

round :: String := vcsv.serialize(t);
t_round :: vcsv.Table := vcsv.parse(round, true);
check("round-trip-rows", t_round.n_rows == 3);
check("round-trip-quoted", vcsv.cell(t_round, 1, 0) == "Grace \"Amazing\" Hopper");
check("round-trip-score", vcsv.cell(t_round, 0, 2) == "99.5");

no_header :: vcsv.Table := vcsv.parse(text, false);
check("no-header-rows", no_header.n_rows == 4);
check("no-header-empty", no_header.headers.size() == 0);

tsv :: String := "a\tb\n1\t2\n";
t_tsv :: vcsv.Table := vcsv.parse_with(tsv, "\t", true);
check("tsv-cols", t_tsv.n_cols == 2);
check("tsv-cell", vcsv.cell(t_tsv, 0, 1) == "2");

check("needs-quote-comma", vcsv.needs_quote("a,b"));
check("needs-quote-none", !vcsv.needs_quote("abc"));
check("quote-field", vcsv.quote_field("a\"b") == "\"a\"\"b\"");

path :: String := "vcsv_test_tmp.csv";
ok :: Bool := vcsv.write_file(t, path);
check("write-file", ok);
check("file-exists", vfs.exists(path));
t_file :: vcsv.Table := vcsv.parse_file(path, true);
check("file-rows", t_file.n_rows == 3);
check("file-cell", vcsv.cell(t_file, 1, 0) == "Grace \"Amazing\" Hopper");
vfs.remove(path);
check("file-removed", !vfs.exists(path));

if failures[0] > 0 {
    out("vcsv: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vcsv: all checks passed");
