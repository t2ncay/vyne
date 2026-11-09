# TODO — Chemistry and drug discovery

**Last updated:** 2026-11-10

Draft. Companion to `todo.md` (Paper 1 pipeline), `todo_2.md` (feature
axes), `todo_optimization.md` (codegen speed), `todo_libraries.md`
(standard library), and `todo_expansion.md` (domain strategy). This file tracks
the compiler prerequisites and the library build-out for the drug discovery
direction under `vchem/`.

**Status (2026-11-10).** `vchem/` is no longer empty: `vchem_common`
(Elements, Constants) and `vgraph` (Types, BFS, DFS, Dijkstra) ship. The rest
of the pipeline (`vmol`, `vsmiles`, `vsmarts`, `vdesc`, `vfp`, `vforce`,
`vconf`, `vdock`, `vcluster`, `vscaffold`, `vpdbfix`) is still absent. The two
general-purpose dependencies this file called blocking, `vcsv` and `vstat`,
have since shipped. Compiler prerequisites C0–C5 are all landed or
substantially landed; C2 retains the non-flattenable struct-ABI residual.

**The original "do not start until Paper 1" gate is historical.** Compiler
prerequisites and the first two `vchem` modules landed while the paper was in
progress. The sequencing advice still applies to downstream libraries: they hit
every corner of the emitter that machine learning does not, so land them in
dependency order.

The through-line: **a chemistry library lives or dies on the cost of
reading a molecule**. A parser that allocates per character, a struct
that boxes per access, an array that cannot hold a static element type —
none of these are visible in a matmul benchmark and all of them dominate
an SDF parse. The compiler work below is what makes the libraries
possible.

---

## Recently landed

This section exists because the compiler prerequisites moved faster than the
rest of the document. It records what actually shipped and is verified by a real
`Molecule { atoms :: Array<Atom>, name :: String, props :: Map }` end-to-end
test. Since this list was written, `vchem_common` and `vgraph` have also
shipped; the rest of the library stages are still future work, but the
compiler-side blockers on most of them are gone.

| Fix                                                                   | Location                                                    | What it unblocks                                                                                                                                                                                                              |
| --------------------------------------------------------------------- | ----------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Dead native-struct-read block removed                                 | `codegen/members.cpp` — `MemberAccessNode::getCExpr`        | Cleanliness. No behavior change. Removed shadowed `recvC`/`recvCt`.                                                                                                                                                           |
| `unboxToNative` handles `Str`/`Map` fields; diagnoses `Array<Struct>` | `codegen/assignments.cpp` — `AssignmentNode::compile`       | Native structs containing `String`, `Map`, or `Array<Struct>` can be declared, assigned, and passed at boxing boundaries. `Array<Struct>` unbox is refused with a Vyne-level diagnostic instead of a silent C error.          |
| Slice 3d extended to non-variable receivers                           | `codegen/members.cpp` — `MemberAssignmentNode::compile`     | Field writes through chained receivers (`o.inner.n = 42`) and index receivers (`mol.atoms[0].x = 99`) reach the real element instead of a boxed throwaway copy. Receiver evaluation hoisted above RHS, matching source order. |
| Empty `[]` in a native-struct constructor                             | `codegen/function_calls.cpp` — `FunctionCallNode::getCExpr` | `Molecule([], "empty", {})` lowers `.atoms = vyne_Array_vyne_Atom_create(0)` instead of `VyneValue arr_N`. Previously a C compile error.                                                                                      |
| Index-lvalue fast path                                                | `codegen/members.cpp` — `MemberAssignmentNode::compile`     | `arr[i].member = v` lowers to `base.data[idx].member = v` directly, bypassing `IndexAccessNode`'s element materialization (which returns a copy, not an lvalue). This closes the "known gap" in C4.                           |

**Verified end-to-end** by `examples/interface_test.vy` (19/19 output
lines correct): struct-of-arrays reads through nested field chains,
field writes through index receivers, single-evaluation of side-effecting
receivers, empty-array construction, `String`/`Map` fields on native
structs, and sibling-field non-interference all pass.

**Known residual issues** (tracked, not blocking):

- `empty_atoms :: Array<Atom> = []` at top level is still declared boxed
  (`VyneValue`) rather than native (`vyne_Array_vyne_Atom`). `.size()`
  works via the boxed fallback; passing it to a native-ABI function
  expecting a `vyne_Array_vyne_Atom*` would fail. Tracked under C4.
- The empty-literal argument in a struct constructor is evaluated once
  before the constructor branch substitutes the native ctor, producing
  a dead `VyneValue arr_N = vyne_array_create(0);` statement. Cosmetic;
  invisible at the current warning level. Cleanup is a mechanical hoist.
- `.atoms` field reads still emit a by-value container copy
  (`vyne_Array_vyne_Atom fld_N = v_mol.atoms;`) before the element
  access. Correct, small (24 bytes), not a profile item.

---

## The scoring test

Same three-question test as `todo.md`, with one addition for chemistry:

| Question                                      | If no, then...                     |
| --------------------------------------------- | ---------------------------------- |
| Does it have a natural per-call boundary?     | Region model doesn't help          |
| Does it do arithmetic on `Int64` / `Float64`? | Native boxing buys you nothing     |
| Does the caller benefit from a peak bound?    | The language's pitch is irrelevant |
| Does it survive a real molecule?              | The library is a toy               |

The fourth question is what separates `vmol` from a textbook exercise.
A molecule is a graph with element types, charges, aromaticity,
stereochemistry, and connectivity. Any library that cannot represent
those correctly on a real ChEMBL compound is a demo, not a tool.

Three or four yeses → strong library. Two → usable but not a demo. One
or fewer → don't.

---

## What ships today

`vchem_common` and `vgraph` ship. What exists that is adjacent:

| Library | State           | Relation to `vchem`                                                                                |
| ------- | --------------- | -------------------------------------------------------------------------------------------------- |
| `vchem_common` | **Done** | Periodic table, radii, masses, constants. |
| `vgraph` | **Done** | CSR graph types, BFS, DFS, Dijkstra. Subgraph isomorphism / MCS still absent. |
| `vbio`  | **Done**        | Biology, not chemistry. Shares `String` representation.                                            |
| `vjson` | **Done**        | Used by `vmol` for SDF data block parsing and by `vchem` for JSON config.                          |
| `vlin`  | **Done**        | Used by `vforce`, `vconf`, and `vdock` for coordinate math.                                        |
| `vfft`  | **Done**        | Used by `vsignal` (vibration analysis) and by spectra handling.                                    |
| `vcsv`  | **Done**        | Required for ChEMBL and ZINC ingestion. Shipped in #109.                                           |
| `vstat` | **Done**        | Required for descriptor statistics and clustering validation. Shipped in #104.                     |
| `vsparse`, `vdecomp`, `vnum` | **Not started** | Required for `vforce`, `vconf`, and solvers. Also tracked in `todo_libraries.md`. |

`vcsv`, `vstat`, `vsparse`, `vdecomp`, and `vnum` are general-purpose and
belong in `modules/external/`, not under `vchem/`. They are listed here because
they are on the critical path. The first two are now satisfied.

---

## Compiler prerequisites — Phase 0

**These were the gate on library work.** All six are now landed or
substantially landed; the section is retained as a record of what each fix
unblocked. Current state (2026-11-10): five of six complete; C2 is substantially
complete with a non-blocking ABI extension outstanding.

### ~~C0. Fix the empty-array literal bug~~ [ DONE ]

Already in progress (see `todo.md` — "empty literal handling"). Blocks
every accumulator pattern. A molecule parser will have dozens of
`result :: Array = [];` inside functions.

**Status.** Landed.

**Effort.** Done.

### ~~C1. Fix generic array element inference~~ [ DONE ]

`[first, ...rest]` should infer element type from `first`. An `[]`
inside a function whose return type is `Array<T>` should infer `T` from
the return annotation. A reassignment `xs = []` inside a function with
`xs :: Array<T>` in scope should infer from the declared type.

**Why.** Every parser returns `Array<Atom>`, `Array<Bond>`,
`Array<Mol>`. Without this, all of them box. An SDF file with 10,000
molecules at 50 atoms each becomes 500,000 boxed allocations instead of
one typed array.

**Status.** Landed.

**Effort.** Done.

**Depends on.** C0.

### C2. Land §6.2 — native array ABI for struct fields [ SUBSTANTIALLY COMPLETE ]

Already in `todo_2.md` as F0 (marked DONE there for the primitive-array
case). The chemistry-specific extension is struct fields.

**Status.** Slices 3a–3g landed. The native-C-struct registry and
typedef emission are live and load-bearing:

- Constructors emit `vyne_*` compound literals.
- Local and global declarations store the native type.
- Field reads and writes lower to `.field` directly (including through
  non-variable receivers after Slice 3d-extension).
- `Array<Struct>` literals build the monomorphized container.
- Index reads return structs by value; index writes through
  `arr[i].member = v` write through the container's `data` pointer
  (index-lvalue fast path, landed this session).
- **Slice 3g landed**: the fixed-point C-eligibility pass in
  `ProgramNode::compile` marks interfaces whose fields are all
  primitives, `String`, `Map`, or `Array<C-eligible-struct>`. Verified
  on `Molecule { atoms :: Array<Atom>, name :: String, props :: Map }`
  — the array field carries `vyne_Array_vyne_Atom`, the string/map
  fields carry bare `VyneValue`, and the constructor emits a compound
  literal of the whole struct.
- `unboxToNative` handles all C-eligible field shapes: primitives by
  coercion, `String`/`Map` by passthrough, `Array<Struct>` refused with
  a Vyne-level diagnostic pending a proper unbox helper.
- Boxing across dynamic boundaries goes through a per-interface
  `vyne_struct_array_box_*` helper.

**Remaining:**

- **Struct-typed native-variant ABI extension.** Functions and methods
  whose parameters are struct-typed still expand to flat fields. That
  works for `Point`-shaped structs but not for `Molecule`-shaped ones
  (which contain `Array<Struct>`, `String`, `Map` and are not
  ABI-flattenable). Not blocking any Stage-1 library — `vmol` calls
  between top-level functions use the boxed ABI, and the boxing cost
  is at the call boundary, not per-field. Track as a Stage-3 concern
  for `vforce`.
- **Test corpus.** The existing regression suite predates Slice 3g. Add
  a struct-of-arrays + `String` + `Map` case to the suite so the
  C-eligibility fixed point cannot silently regress.
- **`empty_atoms :: Array<Atom> = []` at top level** still boxes. The
  local branch of `AssignmentNode::compile` guards the primitive fast
  path on `elem != VType::Struct`; the fall-through boxes the empty
  literal. A native-empty-ctor substitution analogous to the constructor
  fix is the missing piece. Live correctness gap, low urgency.

**Effort.** 1 week for the ABI extension; 1 day each for the test corpus
and the empty-literal native fix.

**Depends on.** Nothing directly. The primitive-array path (F0 in
`todo_2.md`) is done; the struct path has plugged in.

### ~~C3. Fix `ForNode::getCExpr` conditional-in-collect bug~~ [ DONE ]

`if/else` inside a `collect` block pushes `vyne_null()` for one branch.
`relu_prime` is the known victim. Chemistry has more:

- Distance cutoffs: `if dist < cutoff { include } else { skip }`
- Ring detection: conditional membership
- Valence checking: `if valid { count } else { error }`
- Hydrogen bond criteria: `if donor and acceptor { distance } else { 0 }`

Any elementwise kernel with a conditional inside a collect loop is
affected.

**Status.** Landed. The wrapper is bound once and `_vyne_array_size` is
used for both the typed and boxed representations; the collect push is
hardened with an explicit null-init temp so every iteration contributes
a value.

**Residual gap.** The native collect fast path still fires only for
`Float64` expression bodies (`collect { x * 2.0 }`) because
`BinOpNode::getStaticType()` cannot infer the iterator type. Chemistry
workloads are Float64-dominated; close the Int64 path only if a
`vgraph` or `vdesc` profile shows an Int64 collect in the hot path.

**Effort.** Done.

**Depends on.** Nothing.

### ~~C4. Struct field unboxing propagates through nested structs~~ [ DONE ]

`Molecule.atoms.data[i].x` should read a `double` directly. Today it
boxes the atom, then boxes the field, then reads it.

**Status.** Landed this session. `mol.atoms[0].element`,
`mol.atoms[1].x`, and `o.inner.n` all read as native primitives through
the full chain without boxing. The `vyne_Atom idx_N = fld.data[i]`
materialization is a by-value copy of the struct (correct, and the
correct cost for element access), followed by `.element` on the local —
one copy per element, not two box-unbox roundtrips.

**What landed.**

- Slice 3g: fixed-point pass marks `Array<C-eligible-struct>` fields as
  C-eligible, and `cFieldTypeFor` produces the container `CType` with
  `nativeName` populated for `cTypeName()`.
- Slice 3c-extension: `MemberAccessNode::getCExpr` accepts non-variable
  receivers whose temp carries `Struct + nativeCStruct`.
- Slice 3d-extension: `MemberAssignmentNode::compile` writes through
  non-variable receivers, hoisting the receiver above the RHS.
- Index-lvalue fast path: `arr[i].member = v` lowers to
  `base.data[idx].member = v`, closing the C4 known-gap that previously
  caused `mol.atoms[i].x = v` to silently no-op.

**Remaining:** none blocking. The `empty_atoms` boxed-declaration gap
tracked under C2 is a _declaration_-time issue, not a field-access issue.

**Effort.** Done.

**Depends on.** C2.

### ~~C5. `byte_at(s, i) -> Int64` built-in~~ [ DONE ]

`str[i]` returns a fresh 1-char `String`. Every character access
allocates. PDB files are fixed-width, 80 columns, tens of thousands of
lines. SDF files have numeric columns parsed character-by-character.

**Status.** Landed. `str.byte_at(i)` returns the raw byte as `Int64`.

**Effort.** Done.

**Depends on.** Nothing.

---

## The folder design

Two-level, matching `vbio` and `vjson`:

```
modules/external/
├── vbio/
│   ├── Codon.vy
│   ├── Sequence.vy
│   ├── Types.vy
│   └── vbio.vy
├── vchem/
│   ├── vchem.vy                    # top-level facade
│   ├── vchem_common/
│   │   ├── Elements.vy             # periodic table, radii, masses
│   │   ├── Constants.vy
│   │   └── vchem_common.vy
│   ├── vmol/
│   │   ├── Types.vy                # Atom, Bond, Molecule
│   │   ├── SDF.vy
│   │   ├── MOL2.vy
│   │   ├── PDB.vy
│   │   ├── XYZ.vy
│   │   └── vmol.vy
│   ├── vsmiles/
│   │   ├── Types.vy
│   │   ├── Tokenizer.vy
│   │   ├── Parser.vy
│   │   ├── Writer.vy
│   │   ├── Canonical.vy
│   │   └── vsmiles.vy
│   ├── vsmarts/
│   │   ├── Types.vy
│   │   ├── Parser.vy
│   │   ├── Match.vy
│   │   └── vsmarts.vy
│   ├── vgraph/
│   │   ├── Types.vy
│   │   ├── BFS.vy
│   │   ├── DFS.vy
│   │   ├── Dijkstra.vy
│   │   ├── SubgraphIso.vy
│   │   ├── MCS.vy
│   │   └── vgraph.vy
│   ├── vdesc/
│   │   ├── Lipinski.vy
│   │   ├── Crippen.vy
│   │   ├── TPSA.vy
│   │   ├── QED.vy
│   │   ├── Tables.vy
│   │   └── vdesc.vy
│   ├── vfp/
│   │   ├── Types.vy
│   │   ├── Morgan.vy
│   │   ├── MACCS.vy
│   │   ├── Path.vy
│   │   ├── Similarity.vy
│   │   └── vfp.vy
│   ├── vforce/
│   │   ├── Types.vy
│   │   ├── UFF.vy
│   │   ├── Charges.vy
│   │   ├── Minimize.vy
│   │   └── vforce.vy
│   ├── vconf/
│   │   ├── DistanceGeometry.vy
│   │   ├── Torsion.vy
│   │   └── vconf.vy
│   ├── vdock/
│   │   ├── Types.vy
│   │   ├── Grid.vy
│   │   ├── Score.vy
│   │   ├── Search.vy
│   │   ├── Prepare.vy
│   │   └── vdock.vy
│   ├── vcluster/
│   │   ├── Butina.vy
│   │   ├── Hierarchical.vy
│   │   └── vcluster.vy
│   └── vscaffold/
│       ├── Murcko.vy
│       └── vscaffold.vy
├── vcsv/                           # general, blocking
├── vstat/                          # general, blocking
├── vlin/
├── vml/
├── vfft/
└── ...
```

**The current top-level `vchem.vy` facade exposes only what actually exists:**
`vchem_common` and `vgraph`. The target day-one facade is still the four
foundational sub-modules (`vchem_common`, `vmol`, `vgraph`, `vsmiles`); add
`vmol` and `vsmiles` as they land, not before. A facade that re-exports a
half-finished `vdock` produces confusing errors.

---

## The library stages

Each entry has a scoring summary, a dependency list, an effort estimate,
and a validation plan against a reference implementation. **Do not start
a library until its dependencies compile, have a test suite, and pass
against the reference.**

### Stage 1 — Ingestion

The foundation. Everything downstream depends on getting a molecule
into memory in a usable representation.

#### `vchem_common` — shared tables and constants

**What.** Periodic table with atomic number, mass, covalent and van der
Waals radii, electronegativity. Bond-length tables. Constants.

**Why first.** Every other sub-module needs this. A molecule is defined
by the properties of its elements.

**What lands.**

- `Elements.vy` — a table indexed by atomic number, plus lookup by
  symbol.
- `Constants.vy` — Avogadro, Boltzmann, standard pressure and
  temperature, common conversion factors.
- `vchem_common.vy` — sub-facade.

**Effort.** 2 days. The table is large but mechanical.

**Depends on.** Nothing.

**Validation.** Cross-check 20 random elements against NIST WebBook.

#### `vmol` — chemical file formats

**What.** Readers and writers for the formats the field actually uses.

- **SDF** (MDL molfile). The standard for ChEMBL, ZINC, DUD-E.
- **MOL2** (Tripos). Common for docking input.
- **PDB**. Protein structures plus small-molecule ligands.
- **XYZ**. Trivial; useful for test data.

**Why second.** Everything downstream reads a molecule from one of
these. A chemistry library that cannot read SDF is not a chemistry
library.

**What lands.**

- `Types.vy` — `Atom`, `Bond`, `Molecule` interfaces.
- `SDF.vy` — V2000 (the common version) and the data block.
- `MOL2.vy` — atoms, bonds, substructure sections.
- `PDB.vy` — ATOM and HETATM records.
- `XYZ.vy` — a two-line header plus coordinates.
- `Writer.vy` for SDF and XYZ (used in tests).

**The `Molecule` representation.** This shape is now verified
end-to-end by the compiler:

```vyne
interface Atom {
    x        :: Float64,
    y        :: Float64,
    z        :: Float64,
    element  :: Int64,      # atomic number
    charge   :: Int64,
    aromatic :: Bool,
    implicit_h :: Int64,
}

interface Bond {
    a     :: Int64,         # atom index
    b     :: Int64,
    order :: Int64,         # 1, 2, 3, or 4 for aromatic
}

interface Molecule {
    atoms   :: Array<Atom>,     # statically typed, not boxed
    bonds   :: Array<Bond>,
    name    :: String,
    props   :: Map,             # SDF data block
}
```

**Effort.** 2–3 weeks. SDF alone is a week because V2000 has extensions
(chiral flags, charge fields, the `M  CHG` property lines) that
accumulate.

**Depends on.** `vchem_common`, `vfs`, `vjson`. Compiler prerequisites
C2, C4, C5 are now satisfied.

**Validation.** Parse every molecule in a ChEMBL sample (10,000
compounds) and compare atom counts, bond counts, and formula against
RDKit.

#### `vgraph` — graph algorithms

**What.** The graph primitives chemistry needs.

- Adjacency list and adjacency matrix representations.
- BFS, DFS, connected components.
- Topological sort (for reaction networks later).
- Dijkstra, Bellman-Ford.
- **Subgraph isomorphism (VF2)** — the substructure search primitive.
- **Maximum common substructure (MCS)** — for scaffold analysis.

**Why third.** `vmol` produces a molecule; `vgraph` gives it an
adjacency representation. Every downstream operation walks that
representation.

**Effort.** 1 week for the core, 3–4 weeks for subgraph iso and MCS.

**Depends on.** Nothing. `vmol` depends on it, not the reverse.

**Validation.** VF2 on a known pattern-matching test set. MCS against
RDKit's `rdFMCS`.

#### `vsmiles` — SMILES parser and writer

**What.** The SMILES grammar.

- Recursive-descent parser: atoms, bonds, branches, ring closures,
  aromaticity, charges, isotopes, stereochemistry.
- Writer (non-canonical).
- **Canonical SMILES via the Morgan algorithm.** Hard problem,
  deferred to a follow-up.

**Why fourth.** ChEMBL ships SMILES, not SDF, for most of its compounds.
A pipeline that cannot parse SMILES is limited to the subset of data
that ships as SDF.

**What lands.**

- `Types.vy` — token, parse tree.
- `Tokenizer.vy` — string to token stream.
- `Parser.vy` — token stream to `Molecule`.
- `Writer.vy` — `Molecule` to string.
- `Canonical.vy` — **deferred**. Ship parser first; canonicalization is
  a separate 2–3 week project.

**Effort.** 3–4 weeks for the parser. Another 2–3 for canonicalization.

**Depends on.** `vmol`, `vchem_common`. Compiler prerequisites C1, C5
are satisfied.

**Validation.** Parse SMILES for a 1,000-compound ChEMBL subset, write
back with the non-canonical writer, and check that a re-parse produces
the same molecular graph (same atoms, same bonds, possibly different
atom order).

### Stage 2 — Descriptors and fingerprints

What makes a molecule a _screening candidate_.

#### `vdesc` — molecular descriptors

**What.**

- Basic: molecular weight, atom counts, bond counts, ring counts.
- Lipinski's rule of five: HBD, HBA, logP, MW.
- **Wildman-Crippen logP** — 68 atom-type contributions plus 6
  correction factors.
- **TPSA (Ertl)** — polar surface area from fragment contributions.
- **QED (Bickerton)** — eight properties combined into a weighted sum.
- Rotatable bond count.
- Fraction of sp3 carbons.

**Why.** These filter a 10-million compound library down to a
manageable screening set. Without them, the pipeline has no
prioritization.

**Effort.** 2–3 weeks. The tables are large but mechanical.

**Depends on.** `vmol`, `vchem_common`, `vgraph`.

**Validation.** Every descriptor cross-checked against RDKit on a
1,000-compound set. Error tolerance: 0.1% for MW, 1% for logP and TPSA.

#### `vfp` — molecular fingerprints

**What.**

- Morgan / ECFP-like circular fingerprints.
- MACCS keys (166-bit).
- RDKit fingerprint (path-based).
- Tanimoto, Dice, cosine similarity.
- Bit-packed storage using `Array<Int64>`.

**Why.** Similarity search is the primary way to find candidate
compounds when a known active is available. Morgan fingerprints are the
standard for this.

**Effort.** 1–2 weeks.

**Depends on.** `vmol`, `vchem_common`, `vgraph`, `vbitset` (a new
general-purpose library).

**Validation.** Tanimoto similarity between RDKit and `vfp` fingerprints
for a 100-compound set must be within 0.01 absolute.

#### `vcluster` — clustering for hit analysis

**What.**

- Butina clustering (Taylor-Butina). The classic cheminformatics method.
- Hierarchical clustering (single, complete, average linkage).
- k-means with a caller-supplied distance function.
- Silhouette score, Davies-Bouldin index.

**Why.** A screening hit list with 1,000 compounds is not actionable.
Clustering collapses it to 20–50 representative scaffolds.

**Effort.** 1 week.

**Depends on.** `vfp`, `vstat`.

**Validation.** Butina clusters compared against RDKit's implementation
on a 100-compound set.

#### `vscaffold` — scaffold analysis

**What.**

- Murcko scaffold extraction (ring systems plus linkers, side chains
  removed).
- Generic scaffold decomposition.
- Scaffold-based clustering.

**Why.** Bemis-Murcko scaffolds are the standard way to describe a
chemical series. Every lead optimization project uses them.

**Effort.** 3–4 days.

**Depends on.** `vmol`, `vgraph` (uses MCS).

**Validation.** Scaffold output compared against RDKit's
`MurckoScaffold`.

### Stage 3 — 3D geometry and physics

What makes a molecule a _physical object_.

#### `vforce` — molecular force fields

**What.**

- UFF (Universal Force Field) — the starting point. Covers the whole
  periodic table.
- Energy terms: bond stretch, angle bend, torsion, van der Waals,
  electrostatics.
- Analytic gradients for every term.
- Gasteiger-Marsili partial charges.

**Why.** Every 3D structure — a docking pose, a minimized conformer, a
docked complex — is the output of a force field. Without `vforce`,
`vconf` and `vdock` do not exist.

**Effort.** 4–6 weeks. UFF is a big table and the gradient derivation
is error-prone. Budget time for validation.

**Depends on.** `vmol`, `vchem_common`, `vsparse`, `vlin`. Compiler
prerequisite C3 is satisfied. The struct-typed native-variant ABI
extension (see C2 remaining work) becomes relevant here if force-field
evaluation is hot enough to need flat-ABI calls between stages.

**Validation.** Single-point energy and gradient for a set of test
molecules compared against a reference UFF implementation (OpenBabel's
`obenergy` or RDKit's `UFFOptimizeMolecule`). Gradient norms should
match to 1e-4.

#### `vconf` — 3D conformer generation

**What.**

- Distance geometry: build a distance matrix from covalent radii and
  constraints, embed into 3D via classical MDS.
- Eigenvalue solver for the embedding step. Jacobi for symmetric
  matrices is small and easy; use that.
- Rotation of rotatable bonds to enumerate conformers.
- ETKDG (RDKit's default) is the modern standard but needs torsional
  preferences from a database. Start with plain distance geometry;
  add torsion preferences as a follow-up.

**Why.** A molecule that has never been crystallized has no 3D
structure. Docking requires one. Conformer generation is how you get
it.

**Effort.** 3–4 weeks.

**Depends on.** `vforce`, `vlin`, `vrand`, `vnum`.

**Validation.** RMSD between generated conformers and crystal
structures from the Cambridge Structural Database or the PDB. The
expectation is that the lowest-energy conformer is within 1.5 Å RMSD
of the crystal structure for 70–80% of test molecules.

#### `vsparse` — sparse matrix operations

**What.**

- CSR (compressed sparse row) and CSC formats.
- Sparse matrix-vector multiply, sparse matrix-matrix multiply.
- Jacobi, Gauss-Seidel, conjugate gradient solvers.

**Why.** Force field Hessians are sparse. Graph Laplacians are sparse.
Any operation on a molecule's adjacency matrix is sparse.

**Effort.** 1 week.

**Depends on.** Nothing.

**Note.** This is a general-purpose library and could live at
`modules/external/vsparse/` rather than under `vchem/`. Chemistry is
its main consumer, so keeping it under `vchem/` for now is fine. If it
becomes used by something outside chemistry, promote it later.

**Validation.** Sparse matvec and matmul compared against a dense
reference implementation on random sparse matrices.

#### `vnum` — numerical methods

**What.**

- Root finding: bisection, Newton, secant, Brent.
- Integration: trapezoidal, Simpson, Gauss-Legendre, adaptive.
- ODE: Euler, midpoint, RK4, adaptive RK45.
- Linear algebra: LU, QR, Cholesky, SVD.
- Eigenvalues: Jacobi for symmetric matrices, power iteration.

**Why.** `vconf` needs the eigenvalue solver. `vforce` minimization
needs line search. `vnum` underlies all of it.

**Effort.** 1–2 weeks.

**Depends on.** `vlin`.

**Note.** General-purpose, like `vsparse`. Lives at the top level once
it stabilizes.

**Validation.** Jacobi eigenvalues against a reference, LU
decomposition against LAPACK on random matrices.

### Stage 4 — Docking

What makes a pipeline a _drug discovery pipeline_.

#### `vdock` — molecular docking engine

**What.**

- Receptor preparation: PDB parsing, protonation at pH 7.4, grid
  generation.
- Grid: precomputed electrostatic and van der Waals potentials on a
  3D lattice (the AutoDock approach).
- Pose search: Monte Carlo with simulated annealing.
- Scoring: Vina's scoring function (steric Gauss, repulsion,
  hydrophobic, hydrogen bond terms).
- Flexible ligand, rigid receptor.

**Why.** This is the flagship. The whole pipeline exists to produce a
ranked list of compounds, and the ranking is the docking score.

**Effort.** 8–12 weeks.

**Depends on.** Every library above. This is the tip of the pyramid.

**Validation.** Run a DUD-E target end to end and compute the ROC AUC.
Target: within 5% of AutoDock Vina's AUC. If within 1%, that is a
strong result.

**Milestone.** The first end-to-end run against a DUD-E target is the
proof that the pipeline works. Everything before this is
infrastructure.

#### `vsmarts` — SMARTS pattern matching

**What.** SMARTS is SMILES plus wildcards, logic, and atom-recursion
operators. Used for functional group recognition and substructure
search.

**Why.** `vchem` (functional group recognition), `vdesc` (some
descriptors), and `vscaffold` (Murcko decomposition) all need SMARTS.

**Effort.** 2 weeks after `vsmiles` works.

**Depends on.** `vsmiles`, `vgraph`.

**Validation.** A standard SMARTS test set (the Daylight test suite if
you can find it) compared against RDKit.

#### `vpdbfix` — PDB structure cleanup

**What.** Missing atoms, alternate locations, waters, ions, chain
breaks. Every real PDB file needs this before docking.

**Why.** Clean PDB files are rare. A pipeline that cannot handle a
PDB with missing side chains cannot run on most targets.

**Effort.** 1 week.

**Depends on.** `vmol`.

**Validation.** Run on 20 diverse PDB structures and check that the
repaired structures produce sensible docking scores.

---

## Milestones

Ordered. Each is a checkpoint that validates the phase before it.

**M1 — Read a ChEMBL compound.** Parse an SDF file into a `Molecule`
with atoms, bonds, and data block. Verify against RDKit.

Requires: C0, C1, C2, C4, C5 — **all satisfied** — plus `vchem_common`
(**shipped**) and `vmol` (**not started**). M1 is the next real milestone.

**M2 — Parse a SMILES string.** Read a SMILES, produce a `Molecule`
with the same graph (atom count, bond count, connectivity) as the RDKit
parse.

Requires: M1, `vgraph`, `vsmiles`.

**M3 — Compute Lipinski's rule of five.** Given a `Molecule`, produce
MW, HBD, HBA, logP. Verify against RDKit within 1%.

Requires: M2, `vdesc`.

**M4 — Compute Morgan fingerprint.** Given a `Molecule`, produce an
ECFP4 fingerprint. Tanimoto similarity against RDKit within 0.01.

Requires: M3, `vfp`, `vbitset`.

**M5 — Generate a 3D conformer.** Given a `Molecule` from SMILES,
produce a 3D structure. RMSD against crystal structure within 1.5 Å
for 70% of test molecules.

Requires: M4, `vforce`, `vconf`, `vnum`, `vsparse`.

**M6 — Dock a compound.** Given a receptor PDB and a ligand SDF,
produce a binding pose and score. Score within a few kcal/mol of Vina
for the same compound.

Requires: M5, `vdock`, `vpdbfix`.

**M7 — Screen a DUD-E target.** Run the full pipeline on a DUD-E target
(actives + decoys), rank by docking score, compute ROC AUC. Within 5%
of Vina.

Requires: M6, `vcluster`, `vcsv`.

**M8 — Publish.** Write up the pipeline, the compiler guarantees, and
the AUC comparison. The compiler work is the contribution; the AUC is
the demonstration.

Requires: M7.

---

## What is not in this todo

Same exclusions as the parent `todo.md`:

- **No web app, no dashboard.** The pipeline produces a ranked list of
  compounds. Consuming that list is out of scope. `vserv` exists; using
  it is a separate project.
- **No ADMET prediction.** That is a machine learning problem and needs
  `vtensor` and the `<diff>` effect. Defer to Stage 5.
- **No retrosynthesis.** That is a research project on its own. Defer
  to year 3 or later.
- **No reaction handling.** Same reason. `vreact` is not in this todo.
- **No GPU codegen.** Reading A (calling cuBLAS) is fine and lives in
  `todo_2.md` as F5. Reading B (writing CUDA kernels) is not part of
  this direction.

---

## Cross-references

- **`todo.md`** — Paper 1 pipeline. Every compiler fix in Phase 0 of
  this file is compatible with the paper; none is a rewrite. C0–C5 are
  all landed or substantially landed and none has touched the paper's
  benchmark surface.
- **`todo_2.md`** — feature axes. C2 (native array ABI) is F0 there.
  The `<diff>` effect is needed for Stage 5, not for Stage 4. The device
  arena (F2–F5) is orthogonal to chemistry — the chemistry pipeline is
  CPU-bound and does not need it.
- **`todo_optimization.md`** — codegen speed. Compiler fixes C1–C5
  affect codegen; run the optimization benchmark before and after each
  to confirm no regression. The Slice 3d-extension and the index-lvalue
  fast path add a branch and a hoisted evaluation to `MemberAssignmentNode::compile`;
  the emitted C for existing tests is unchanged except for the shapes
  that previously produced a throwaway copy.
- **`todo_libraries.md`** (standard library) — `vcsv` and `vstat` are on the
  critical path for chemistry and have shipped; `vsparse`, `vdecomp`, and
  `vnum` are still queued there.

---

## Don't do this

- **Don't start a library before its dependencies are done.** The
  dependency graph is a DAG. Walking it out of order produces a library
  that has to be rewritten when its dependency lands.
- **Don't write your own InChI.** The spec is enormous and the
  reference implementation is the only one that matters. Skip InChI.
- **Don't write your own canonical SMILES before writing the parser.**
  Canonicalization is a graph-isomorphism problem. Get the parser
  working first; the canonical writer is a follow-up.
- **Don't try to match RDKit's API.** RDKit's API is a Python-C++
  hybrid that makes sense for its context. Vyne's should be simpler,
  more functional, and use the language's own idioms.
- **Don't skip validation against a reference.** Every library in this
  todo has an explicit validation step. Skipping it means discovering
  a subtle chemistry bug six months later when a docking score is wrong
  by a factor of three.
- **Don't optimize the parser before it is correct.** ChEMBL has edge
  cases. ZINC has more. A fast parser that gets the wrong answer is
  worse than a slow one that gets the right answer.
- **Don't build `vdock` before `vforce` and `vconf`.** Docking without
  a force field is a grid search through a vacuum. It will not
  reproduce Vina's scores and it will not produce meaningful poses.
- **Don't use `vchem` as a name for a single library.** It is the
  facade. The leaves have their own names.
- **Don't put general-purpose modules under `vchem/`.** `vcsv` and
  `vstat` belong at the top level. So does `vsparse` once it has more
  than one consumer.
- **Don't do all of Stage 1 in parallel.** `vmol` needs `vchem_common`
  done first. `vgraph` needs nothing but is used by `vmol`. `vsmiles`
  needs `vmol` done. Serialize.
- **Don't extend the struct-typed native-variant ABI (C2 remaining)
  until a `vforce` profile demands it.** The boxed ABI works for
  everything in Stage 1 and Stage 2. The flat ABI is a Stage-3
  optimization, not a prerequisite.

---

## What "done" looks like for this file

When you can run the following and get a number to compare against
Vina:

```
vynec screen.vy \
    --receptor target.pdb \
    --ligands chembl_100k.sdf \
    --output ranked.csv
```

and `screen.vy`:

1. Reads a receptor from PDB, protonates it, generates a grid.
2. Reads 100,000 ligands from SDF.
3. Filters by Lipinski's rule of five.
4. Pre-filters by Morgan fingerprint similarity to known actives.
5. Generates 3D conformers for the top 10,000.
6. Docks each conformer.
7. Ranks by score.
8. Writes a CSV.

...in under an hour on a modern CPU, with peak memory that is a
compile-time number, then this file is done.

The paper that comes out of it: **"A single-language virtual screening
pipeline with compile-time memory bounds."** The compiler guarantees
are the contribution; the pipeline is the demonstration; the AUC is
the number reviewers can compare.

Two more things worth writing down: the number of compounds screened
per second, and the peak memory during screening. Those are the two
metrics that distinguish Vyne from the Python + C++ + CUDA stack that
everyone else uses. If the numbers are good, the paper writes itself.
