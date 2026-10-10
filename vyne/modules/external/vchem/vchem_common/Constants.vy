# vchem_common/Constants.vy — physical constants for molecular work.
#
# SI units unless the name says otherwise. Values from the 2019 SI
# redefinition, CODATA 2018. Nothing computed at runtime; every entry
# is a literal.

ruleset { dynamic_casting };

module vchem;

# Avogadro constant, mol^-1.
AVOGADRO :: Float64 = 6.02214076e23;

# Boltzmann constant, J/K.
BOLTZMANN :: Float64 = 1.380649e-23;

# Universal gas constant, J/(mol K).
R_GAS :: Float64 = 8.314462618;

# Standard pressure, Pa.
STD_PRESSURE :: Float64 = 101325.0;

# Standard temperature, K (273.15 K = 0 °C).
STD_TEMPERATURE :: Float64 = 273.15;

# Normal boiling point of water at 1 atm, K.
WATER_BOILING_K :: Float64 = 373.15;

# Body temperature, K.
BODY_TEMPERATURE_K :: Float64 = 310.15;

# kT at body temperature, J. Common in binding-affinity conversions.
KT_BODY :: Float64 = 4.2803e-21;

# kcal -> kJ conversion.
KCAL_TO_KJ :: Float64 = 4.184;

# kcal/mol -> kT at 300 K. Typical thermal energy at room temperature.
KCAL_PER_MOL_TO_KT_300 :: Float64 = 1.6782;

# Angstrom -> nanometre.
ANGSTROM_TO_NM :: Float64 = 0.1;

# Angstrom -> metre.
ANGSTROM_TO_M :: Float64 = 1.0e-10;

# Degree -> radian.
DEG_TO_RAD :: Float64 = 0.017453292519943295;

# Radian -> degree.
RAD_TO_DEG :: Float64 = 57.29577951308232;

# Two pi. Common in rotational/torsional terms.
TWO_PI :: Float64 = 6.283185307179586;

# Golden ratio. Rarely used; kept for the occasional packing heuristic.
PHI :: Float64 = 1.6180339887498949;
