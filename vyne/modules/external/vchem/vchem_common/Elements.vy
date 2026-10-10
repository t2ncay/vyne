# vchem_common/Elements.vy — periodic table with masses, radii, and
# Pauling electronegativity.
#
# Data sources:
#   mass       — IUPAC 2021 standard atomic weights (abridged)
#   covalent_r — Cordero et al. 2008, single-bond covalent radii, Angstroms
#   vdw_r      — Bondi 1964 + Mantina 2009 extensions, Angstroms
#   EN         — Pauling electronegativity, dimensionless
#
# We ship elements 1..54 (H through Xe). That range covers the full
# main group plus the first transition row, which is where essentially
# all drug-discovery chemistry lives. Elements 55..118 are a mechanical
# extension of the same tables; adding them requires no API change.
#
# All numeric tables are native Array<Float64>, so atomic_mass(z)
# returns a bare double. Symbol and name tables are boxed arrays of
# String; access to them goes through the boxed path, which is fine
# because they are only touched during I/O and diagnostics.

ruleset { dynamic_casting };

module vchem;

# Index 0 is a sentinel so `TABLE[z]` works for z in 1..54 with no
# off-by-one correction. Every numeric table has the same length.

MASS :: Array<Float64> = [
    0.0,
    1.008,    4.0026,   6.94,     9.0122,  10.81,
    12.011,   14.007,   15.999,   18.998,  20.180,
    22.990,   24.305,   26.982,   28.085,  30.974,
    32.06,    35.45,    39.948,   39.098,  40.078,
    44.956,   47.867,   50.942,   51.996,  54.938,
    55.845,   58.933,   58.693,   63.546,  65.38,
    69.723,   72.630,   74.922,   78.971,  79.904,
    83.798,   85.468,   87.62,    88.906,  91.224,
    92.906,   95.95,    98.0,     101.07,  102.91,
    106.42,   107.87,   112.41,   114.82,  118.71,
    121.76,   127.60,   126.90,   131.29
];

COVALENT_R :: Array<Float64> = [
    0.0,
    0.31,     0.28,     1.28,     0.96,    0.84,
    0.76,     0.71,     0.66,     0.57,    0.58,
    1.66,     1.41,     1.21,     1.11,    1.07,
    1.05,     1.02,     1.06,     2.03,    1.76,
    1.70,     1.60,     1.53,     1.39,    1.39,
    1.32,     1.26,     1.24,     1.32,    1.22,
    1.22,     1.20,     1.19,     1.20,    1.20,
    1.16,     2.20,     1.95,     1.90,    1.75,
    1.64,     1.54,     1.47,     1.46,    1.42,
    1.39,     1.45,     1.44,     1.42,    1.39,
    1.39,     1.38,     1.39,     1.40
];

VDW_R :: Array<Float64> = [
    0.0,
    1.20,     1.40,     1.82,     1.53,    1.92,
    1.70,     1.55,     1.52,     1.47,    1.54,
    2.27,     1.73,     1.84,     2.10,    1.80,
    1.80,     1.75,     1.88,     2.75,    2.31,
    2.30,     2.15,     2.05,     2.05,    2.05,
    2.05,     2.00,     2.00,     2.00,    2.10,
    1.87,     2.11,     1.85,     1.90,    1.85,
    2.02,     3.03,     2.49,     2.40,    2.30,
    2.15,     2.10,     2.05,     2.05,    2.00,
    2.05,     2.10,     2.20,     2.20,    2.25,
    2.20,     2.06,     1.98,     2.16
];

EN :: Array<Float64> = [
    0.0,
    2.20,     0.0,      0.98,     1.57,    2.04,
    2.55,     3.04,     3.44,     3.98,    0.0,
    0.93,     1.31,     1.61,     1.90,    2.19,
    2.58,     3.16,     0.0,      0.82,    1.00,
    1.36,     1.54,     1.63,     1.66,    1.55,
    1.83,     1.88,     1.91,     1.90,    1.65,
    1.81,     2.01,     2.18,     2.55,    2.96,
    3.00,     0.82,     0.95,     1.22,    1.33,
    1.60,     2.16,     1.90,     2.20,    2.28,
    2.20,     1.93,     1.69,     1.78,    1.96,
    2.05,     2.10,     2.66,     2.60
];

SYM :: Array = [
    "",   "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",
    "Ne", "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",
    "Ca", "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu",
    "Zn", "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y",
    "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In",
    "Sn", "Sb", "Te", "I",  "Xe"
];

NAME :: Array = [
    "",          "Hydrogen",   "Helium",      "Lithium",     "Beryllium",
    "Boron",     "Carbon",     "Nitrogen",    "Oxygen",      "Fluorine",
    "Neon",      "Sodium",     "Magnesium",   "Aluminium",   "Silicon",
    "Phosphorus","Sulfur",     "Chlorine",    "Argon",       "Potassium",
    "Calcium",   "Scandium",   "Titanium",    "Vanadium",    "Chromium",
    "Manganese", "Iron",       "Cobalt",      "Nickel",      "Copper",
    "Zinc",      "Gallium",    "Germanium",   "Arsenic",     "Selenium",
    "Bromine",   "Krypton",    "Rubidium",    "Strontium",   "Yttrium",
    "Zirconium", "Niobium",    "Molybdenum",  "Technetium",  "Ruthenium",
    "Rhodium",   "Palladium",  "Silver",      "Cadmium",     "Indium",
    "Tin",       "Antimony",   "Tellurium",   "Iodine",      "Xenon"
];

# ----------------------------------------------------------------------
# Accessors. All bounds-checked; an out-of-range Z returns the sentinel
# at index 0 rather than the undefined behaviour a bare index would
# produce. The upper bound is data-driven: if you extend the tables,
# update this constant.
# ----------------------------------------------------------------------
MAX_Z :: Int64 = 54;

fn :: vchem atomic_mass(z :: Int64) -> Float64 {
    if z < 0 || z > vchem.MAX_Z { return 0.0; }
    return vchem.MASS[z];
}

fn :: vchem covalent_radius(z :: Int64) -> Float64 {
    if z < 0 || z > vchem.MAX_Z { return 0.0; }
    return vchem.COVALENT_R[z];
}

fn :: vchem vdw_radius(z :: Int64) -> Float64 {
    if z < 0 || z > vchem.MAX_Z { return 0.0; }
    return vchem.VDW_R[z];
}

fn :: vchem electronegativity(z :: Int64) -> Float64 {
    if z < 0 || z > vchem.MAX_Z { return 0.0; }
    return vchem.EN[z];
}

fn :: vchem symbol(z :: Int64) -> String {
    if z < 0 || z > vchem.MAX_Z { return "X"; }
    return vchem.SYM[z];
}

fn :: vchem name(z :: Int64) -> String {
    if z < 0 || z > vchem.MAX_Z { return "unknown"; }
    return vchem.NAME[z];
}

# Linear scan of the 54-entry symbol table. Fast enough — the
# symbol-to-Z direction is only touched when parsing input files,
# not during numerical work. A Map<String, Int64> would be faster
# but the construction boilerplate is not worth it for 54 entries.
fn :: vchem atomic_number(sym :: String) -> Int64 {
    through i :: 1..vchem.MAX_Z -> loop {
        if vchem.SYM[i] == sym { return i; }
    };
    return 0;
}
