# data.vy - Iris loader for srcnn2
#
# The dataset is bundled at data/iris.data (the UCI iris.data file with
# a normalised trailing newline). One row is:
#
#   sepal_length,sepal_width,petal_length,petal_width,species
#
# Features are used as-is; labels become one-hot vectors in the order
# setosa, versicolor, virginica.

use native vfs;
use external "vcsv/vcsv.vy";
use external "vlin/vlin.vy";

ruleset { dynamic_casting };

module data;

interface Dataset {
    X        :: vlin.Types.Matrix,
    Y        :: vlin.Types.Matrix,
    n        :: Int64,
    features :: Int64,
    classes  :: Int64,
}

fn :: data load_iris(path :: String) -> data.Dataset {
    # The upstream UCI file ends with a blank line. Strip trailing
    # CR/LF bytes before parsing so the last empty line is not treated
    # as a one-column row.
    text :: String := vfs.read(path);
    n_chars :: Int64 := text.length();
    while n_chars > 0 {
        last :: Int64 := text.byte_at(n_chars - 1);
        if last != 10 && last != 13 { break; }
        n_chars = n_chars - 1;
    }
    clean :: String := text.substr(0, n_chars);
    table :: vcsv.Table := vcsv.parse_with(clean, ",", false);
    if table.n_cols != 5 {
        throw "data.load_iris: expected 5 columns, found " +
            string(table.n_cols);
    }

    n :: Int64 := table.n_rows;
    features :: Int64 := 4;
    classes :: Int64 := 3;

    X_flat :: Array<Float64> := [];
    Y_flat :: Array<Float64> := [];

    through r :: 0..n-1 -> loop {
        through c :: 0..features-1 -> loop {
            X_flat.push(float64(vcsv.cell(table, r, c)));
        };

        label :: String := vcsv.cell(table, r, 4);
        s :: Float64 := 0.0;
        v :: Float64 := 0.0;
        g :: Float64 := 0.0;
        if label == "Iris-setosa" {
            s = 1.0;
        } else if label == "Iris-versicolor" {
            v = 1.0;
        } else if label == "Iris-virginica" {
            g = 1.0;
        } else {
            throw "data.load_iris: unknown species '" + label + "'";
        }
        Y_flat.push(s);
        Y_flat.push(v);
        Y_flat.push(g);
    };

    X :: vlin.Types.Matrix := vlin.Types.Matrix(n, features, X_flat);
    Y :: vlin.Types.Matrix := vlin.Types.Matrix(n, classes, Y_flat);
    return data.Dataset(X, Y, n, features, classes);
}
