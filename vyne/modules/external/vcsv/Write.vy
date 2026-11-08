# vcsv/Write.vy - RFC 4180-style serializer.

use "Types.vy";
use native vcore;
use native vfs;

ruleset { dynamic_casting };

module vcsv;

fn :: vcsv needs_quote(s :: String) -> Bool {
    if s.find(",") >= 0 { return true; }
    if s.find("\"") >= 0 { return true; }
    if s.find("\n") >= 0 { return true; }
    if s.find("\r") >= 0 { return true; }
    return false;
}

fn :: vcsv quote_field(s :: String) -> String {
    escaped :: String := s.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}

fn :: vcsv append_field(sb :: Int64, s :: String) -> Int64 {
    if vcsv.needs_quote(s) {
        vcore.sb_append(sb, vcsv.quote_field(s));
    } else {
        vcore.sb_append(sb, s);
    }
    return 0;
}

fn :: vcsv serialize_with(t :: vcsv.Table, delim :: String) -> String {
    sb :: Int64 := vcore.sb_create();

    if t.headers.size() > 0 {
        through c :: 0..t.n_cols-1 -> loop {
            if c > 0 { vcore.sb_append(sb, delim); }
            vcsv.append_field(sb, t.headers[c]);
        };
        vcore.sb_append(sb, "\n");
    }

    through r :: 0..t.n_rows-1 -> loop {
        through c2 :: 0..t.n_cols-1 -> loop {
            if c2 > 0 { vcore.sb_append(sb, delim); }
            vcsv.append_field(sb, t.cells[r * t.n_cols + c2]);
        };
        vcore.sb_append(sb, "\n");
    };
    return vcore.sb_build(sb);
}

fn :: vcsv serialize(t :: vcsv.Table) -> String {
    return vcsv.serialize_with(t, ",");
}

fn :: vcsv write_file_with(t :: vcsv.Table, path :: String,
                           delim :: String) -> Bool {
    return vfs.write(path, vcsv.serialize_with(t, delim));
}

fn :: vcsv write_file(t :: vcsv.Table, path :: String) -> Bool {
    return vfs.write(path, vcsv.serialize(t));
}
