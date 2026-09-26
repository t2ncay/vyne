# vbio

Molecular biology primitives for Vyne.

**Version:** 0.1.0
**Status:** unstable — public surface may change between releases
**Requires:** nothing beyond the Vyne standard library

---

## Overview

`vbio` is a small, pure-Vyne library for working with nucleotide and
protein sequences. It provides the primitives that sequence-analysis
code reaches for first: complement and reverse-complement, DNA-to-RNA
transcription, codon-table translation, GC content, melting temperature,
and the two or three sequence-composition helpers you would otherwise
hand-roll.

The library deliberately omits alignment, phylogenetic comparison,
secondary-structure prediction, and anything that needs a scoring
matrix. Those are their own fields, and each one is large enough to
deserve its own module. What `vbio` provides is the layer underneath:
the operations that every downstream tool builds on, written to be
read and reimplemented rather than called blindly.

Sequences are plain Vyne `String`s. The `DNA`, `RNA`, and `Protein`
types are thin interfaces over a String plus a label; they exist to
make intent explicit at the type level, not to hide the underlying
character data. Any function in the library that takes a raw `String`
will accept the value of a `DNA`, `RNA`, or `Protein` via `.seq`. This
keeps the library usable both from typed code that wants the named
types and from untyped code that wants to treat sequences as strings.

The codon table is an mRNA table (U, not T). If your source is DNA,
transcribe it first with `bio.transcribe`. The library does not carry
two parallel tables for the two alphabets; there is one table, and the
transcription step is a three-line function you can read in ten seconds.

---

## Quick start

```vyne
use lib "vbio/bio.vy";

module bio;

# Direct string-level operations
bio.gc_content("GCGCATAT")              # 0.5
bio.complement("ATGC")                  # "TACG"
bio.reverse_complement("ATGC")          # "GCAT"

# Typed wrappers
d :: bio.Types.DNA = bio.make_dna("ATGGCTTAA");
d.describe()                            # "unnamed (9 bp)"

# Transcription and translation
r :: bio.Types.RNA = bio.transcribe(d);
p :: bio.Types.Protein = bio.translate_rna(r);
p.seq                                   # "MA*" — Met, Ala, stop

# Composition
bio.codon_usage("AUGGCUUAA")            # Map: {"AUG":1, "GCU":1, "UAA":1}
bio.melting_temperature("ATGCATGCAT")   # 28.0 (Wallace rule)
```

Nothing in this library mutates its input. Every operation returns a
fresh String, Map, or interface value.

---

## The `DNA`, `RNA`, and `Protein` types

All three are the same shape:

```vyne
interface DNA {
    seq  :: String,
    name :: String,
}

interface RNA {
    seq  :: String,
    name :: String,
}

interface Protein {
    seq  :: String,
    name :: String,
}
```

The `seq` field holds the raw characters. The `name` field is a label
the caller controls; nothing in the library interprets it beyond
including it in `describe()` output.

The three types are distinct at the type level, so a function that
takes `bio.Types.DNA` will reject an RNA. This is useful when you have a
pipeline of operations that only makes sense in one alphabet, and it is
the only reason the three interfaces exist. If you do not want that
safety, use plain `String`s and skip the wrappers entirely.

### Alphabet assumptions

The library assumes but does not enforce:

| Type      | Alphabet                                     |
| --------- | -------------------------------------------- |
| `DNA`     | `A`, `C`, `G`, `T`                           |
| `RNA`     | `A`, `C`, `G`, `U`                           |
| `Protein` | 20 standard 1-letter codes plus `*` for stop |

Ambiguity codes (`N`, `R`, `Y`, `W`, etc.) are not part of any alphabet
and will not be translated meaningfully. `bio.codon_to_aa` returns `"X"`
for any codon it does not recognize, and `bio.complement_base` returns
`"N"` for any base it does not recognize, so an ambiguous input degrades
to an ambiguous output rather than crashing.

### Method reference

The three interfaces share the same two methods.

| Method         | Returns  | Notes                                                 |
| -------------- | -------- | ----------------------------------------------------- |
| `x.length()`   | `Int64`  | Number of characters in `x.seq`                       |
| `x.describe()` | `String` | `"<name> (<length> bp/nt/aa)"` — units depend on type |

`describe()` is purely cosmetic. `DNA` reports `bp`, `RNA` reports `nt`,
`Protein` reports `aa`. If you need the length without a unit suffix,
use `.length()` or `.seq.size()` directly.

---

## Importing

```vyne
use lib "vbio/bio.vy";
module bio;
```

`bio.vy` is a facade over three leaf modules:

| Module        | Contents                                                   |
| ------------- | ---------------------------------------------------------- |
| `Types.vy`    | `DNA` / `RNA` / `Protein` interfaces and factories         |
| `Codon.vy`    | Codon table, translation, reverse-translation, codon usage |
| `Sequence.vy` | Complement, reverse-complement, transcription, composition |

You can import the leaf modules individually if you want to avoid
pulling in the full surface, but the facade is the recommended entry
point and the three leaves are small enough that the difference rarely
matters.

---

## API reference

### Factories

Constructors for the three interface types. These exist so you do not
have to spell out `bio.Types.DNA(seq, "unnamed")` at every call site.

```vyne
bio.make_dna(seq :: String) -> bio.Types.DNA
bio.make_rna(seq :: String) -> bio.Types.RNA
bio.make_protein(seq :: String) -> bio.Types.Protein
```

All three set `name` to `"unnamed"`. To give a sequence a meaningful
label, construct the interface directly:

```vyne
d :: bio.Types.DNA = bio.Types.DNA("ATGC", "sample_001");
```

### The genetic code

The codon table lives at module scope as two plain `Map`s, built once at
program start and shared across every caller.

```vyne
const bio_rna_codons :: Map
const bio_aa_to_codon :: Map
```

`bio_rna_codons` maps every mRNA codon to its 1-letter amino-acid code,
using `*` for the three stop codons. `bio_aa_to_codon` maps each of the
20 standard amino acids back to a single representative codon, biased
toward E. coli K-12 usage. Neither map is meant to be edited at runtime;
if you need a different codon bias, edit the source and recompile.

The accessors:

```vyne
bio.codon_to_aa(codon :: String) -> String
bio.is_stop_codon(codon :: String) -> Bool
bio.codon_for(aa :: String) -> String
```

`codon_to_aa` returns `"X"` for any input that is not a recognized
3-character codon. `is_stop_codon` is a thin wrapper around
`codon_to_aa(c) == "*"`, provided so call sites read cleanly.
`codon_for` returns `"NNN"` for any amino acid not in the reverse
table; it is the safe default when building a reverse-translated
sequence from an unknown input.

### Translation

```vyne
bio.translate(mrna :: String) -> bio.Types.Protein
bio.translate_rna(r :: bio.Types.RNA) -> bio.Types.Protein
```

`translate` reads the input in 3-character windows, left to right,
starting at index 0. It stops at the first stop codon and drops
anything after it. A trailing window shorter than 3 characters is
ignored. The result is wrapped in a `Protein` whose `name` is
`"translated"`.

`translate_rna` is the same operation on an `RNA` value. The result's
`name` is `r.name + "_protein"`; the `.seq` is identical to what
`translate(r.seq)` would produce.

There is no reading-frame selection. If you want frame 1 or frame 2,
slice the input yourself (`s[1:]`, `s[2:]`) before calling `translate`.
The library does not offer a reading-frame API because "translate in
all six frames and pick the longest ORF" is a policy question that
belongs in the caller, not in a table-lookup function.

### Reverse translation

```vyne
bio.reverse_translate(protein :: String) -> String
```

Produces one possible mRNA for the given protein, using the codon
choice in `bio_aa_to_codon` for each residue. Appends `"UAA"` as a
terminal stop.

The result is not unique. Most amino acids are encoded by multiple
codons, and any of the alternatives would be a valid reverse
translation. The choice of codon per residue is fixed by
`bio_aa_to_codon`; if you want a different bias, change the map.

This function is useful for generating test data, for building
expression cassettes against a specific host's codon bias, and for
producing sequences that are guaranteed to translate back to the same
protein under the standard code. It is not useful for inferring the
original mRNA from a protein, because there is no information in the
protein to recover that.

### Nucleotide operations

```vyne
bio.complement_base(b :: String) -> String
bio.complement(seq :: String) -> String
bio.reverse_string(seq :: String) -> String
bio.reverse_complement(seq :: String) -> String
```

`complement_base` maps `A↔T`, `G↔C`, and `U→A`. Anything else — including
ambiguous codes and any lowercase input — becomes `"N"`. The mapping is
not symmetric for `U`: transcribing RNA back to DNA would require
`U→A`, which it does, but there is no `T→U` here; use
`bio.transcribe` for that.

`complement` applies `complement_base` character by character. It does
not check that the input alphabet is consistent; feeding it a mix of
DNA and RNA bases will produce a mix of DNA and RNA complements, which
is almost certainly not what you want. Pick an alphabet at the top of
your program and stay in it.

`reverse_string` reverses the characters of the input. It is a String
operation, not a biological one; it is exported because
`reverse_complement` needs it and because sequence code seems to need
it often enough that hiding it would be annoying.

`reverse_complement` is `reverse_string(complement(seq))`. It is the
most common operation in sequence work and is provided as a single
function so you do not have to compose it yourself.

### DNA / RNA conveniences

```vyne
bio.rc(d :: bio.Types.DNA) -> bio.Types.DNA
bio.transcribe(d :: bio.Types.DNA) -> bio.Types.RNA
```

`rc` is the reverse complement of a DNA value, wrapped in a new `DNA`
whose `name` is `d.name + "_rc"`. It exists alongside
`reverse_complement` for callers who want to stay in the typed world
end-to-end.

`transcribe` replaces every `T` with `U` and wraps the result in an
`RNA` whose `name` is `d.name + "_mrna"`. Every other character passes
through unchanged.

### Composition

```vyne
bio.count_base(seq :: String, b :: String) -> Int64
bio.gc_content(seq :: String) -> Float64
bio.melting_temperature(seq :: String) -> Float64
```

`count_base` counts exact single-character matches. It does not
normalize case. If your sequences come from a source that might mix
case, upper-case them first.

`gc_content` returns `(G + C) / N`. Returns `0.0` for an empty input.
It does not consider ambiguity codes; an input containing `N` will have
its denominator inflated without a matching numerator, which is the
conservative choice.

`melting_temperature` uses two rules depending on length:

- **Length < 14 nt** — the Wallace rule,
  `Tm = 2·(A + T) + 4·(G + C)`.
- **Length ≥ 14 nt** — a GC-based approximation,
  `Tm = 64.9 + 41·(G + C − 16.4) / N`.

Both rules assume a salt concentration of roughly 50 mM Na⁺ and a
primer concentration around 250 nM. If you are working outside that
window, the number this function returns will be wrong by a few
degrees. It is a first-pass estimate for primer design, not a
substitute for a nearest-neighbour thermodynamic model.

### Codon composition

```vyne
bio.codon_usage(mrna :: String) -> Map
```

Splits the input into non-overlapping 3-character windows starting at
index 0, and returns a `Map` from codon to count. Any trailing window
shorter than 3 characters is ignored, matching `translate`.

The result is a plain `Map` whose keys are 3-character `String`s. There
is no special type; this is deliberate, because the map is most useful
when it can be passed straight to `has`, `keys`, and the other Map
methods without conversion.

### Search

```vyne
bio.find_first(seq :: String, substr :: String) -> Int64
bio.hamming_distance(a :: String, b :: String) -> Int64
```

`find_first` returns the 0-based index of the first occurrence of
`substr` in `seq`, or `-1` if there is none. An empty `substr` returns
`0` (the first position where an empty pattern matches, by convention).
`find_first` is O(n·m) with no preprocessing; for repeated searches
against the same text, build a suffix automaton or a KMP failure table
in your own code.

`hamming_distance` requires equal-length inputs and returns `-1` if the
lengths differ. Equal-length mismatch is a common pattern in
sequencing (unaligned reads against a reference of the same length);
unequal-length mismatch is not, and the caller almost certainly wants
to know rather than silently getting a wrong number.

---

## Reference semantics

Vyne strings are immutable. Every function in `vbio` returns a fresh
`String`, `Map`, or interface value; none of them mutate their input.
There is no aliasing to reason about.

The one thing to be aware of is that interface values are passed by
reference, like everything else in Vyne. If you write:

```vyne
fn mangle(d :: bio.Types.DNA) {
    d.seq = "AAAA";       # does not do what you think
}
```

... you are not mutating the caller's DNA. `d.seq = ...` rebinds the
`seq` field of the local handle, which is a reference to the caller's
interface object. Whether this affects the caller depends on whether
`seq` is stored by value or by reference in the interface's backing
struct; in the current implementation it is stored as a boxed
`VyneValue`, and assigning to it writes through. Do not rely on
either behaviour. If you want a modified DNA, construct a new one:

```vyne
fn rename(d :: bio.Types.DNA, newName :: String) -> bio.Types.DNA {
    return bio.Types.DNA(d.seq, newName);
}
```

This is the same reference-semantics caveat that `vlinalg` documents
for its `Matrix` type. The two libraries make the same trade-off, for
the same reason: the interface values are lightweight wrappers, and
copying the `seq` field on every construction would be wasteful for
long sequences.

---

## Memory model

Every function in `vbio` that returns a `String` allocates from the
arena. For the sequence lengths this library is designed for (tens to
hundreds of bases), the allocation is negligible. For thousands of
bases, the arena cost is real but still small compared to the cost of
the string-building loops themselves.

Inside a training or analysis loop, wrap the body in a `vmem` region:

```vyne
module vmem;

through seq :: sequences -> loop {
    cp = vmem.checkpoint();
    # every vbio call in here allocates
    vmem.rewind(cp);
};
```

The library does not cache anything. Codon tables are built once at
program start; everything else is computed on demand. If you find
yourself calling `bio.transcribe(d)` repeatedly on the same `d`, cache
the result yourself.

---

## Limitations

The library is intentionally small. The following constraints are
known and are unlikely to change without a version bump.

### No reading frames

`bio.translate` always reads frame 0. If you want ORF finding, six-frame
translation, or reading-frame selection, you are writing that code
yourself. The library gives you `translate` and the codon table; the
policy on top of them is yours.

### No alignment

There is no Smith-Waterman, no Needleman-Wunsch, no local alignment, no
global alignment. Alignment is a large topic with its own parameter
space (gap penalties, scoring matrices, affine vs linear gaps) and
belongs in a separate module. `bio.hamming_distance` is provided
because it is a one-line function that unaligned-read workflows need
constantly; anything more is out of scope.

### No secondary structure

No folding, no minimum-free-energy computation, no RNA secondary
structure prediction. These are research-grade algorithms (Nussinov,
Zuker, McCaskill) and none of them belong in a general-purpose sequence
utility library.

### No ambiguity codes

The alphabets the library enforces by convention are `ACGT` for DNA,
`ACGU` for RNA, and the 20 standard amino acids plus `*` for protein.
IUPAC ambiguity codes are not handled. An input containing `N` will
translate as if each `N` were a separate unknown base; the codon table
will return `"X"` for any codon containing `N`, and the reverse table
will return `"NNN"` for any amino acid outside the standard 20.
This is the safe degradation, but it is not the same as handling
ambiguity properly.

### Case sensitivity

`bio.codon_to_aa` and the other table lookups are case-sensitive.
Lowercase input is not recognized and will produce `"X"`, `"N"`, or
`"NNN"` depending on which function you call. Upper-case your input
before passing it to the library, or accept the fact that
`bio.translate("auggcuuaa")` will return an empty protein.

### Codon bias is E. coli K-12

`bio_aa_to_codon` is hard-coded to the E. coli K-12 codon usage table.
If you are reverse-translating for a different host, edit the map. The
map is at the top of `Codon.vy` and is a plain `Map` literal; changing
it is a matter of replacing twenty string values.

### No annotation or metadata

The interfaces carry a `name` field, but nothing else. There is no way
to attach a description, a source, a reference genome coordinate, or a
quality score to a sequence. If you need rich metadata, hold your own
record type alongside the `vbio` sequence.

### Performance

Every operation is a character-by-character loop with no vectorization
and no lookup-table optimizations beyond the codon `Map`s. For
sequences up to a few thousand characters, the constant factor is
negligible. For genome-scale work, this library is the wrong tool;
you want a C or Rust implementation with SIMD character classification
and indexed suffix structures.

If you need fast sequence search at scale, use `bio.find_first` as a
specification of the semantics and then call out to a native library
through the `extern` module system.

---

## Design notes

A few choices that are worth understanding if you plan to extend the
library.

### Why sequences are `String`, not `Array<String>`

A sequence is a sequence of characters, and a Vyne `String` is a
sequence of characters. Using `Array<String>` would mean allocating a
`VyneValue` per character, which is roughly 24 bytes per base in the
current runtime. For a 10 kb sequence that is 240 kB of arena traffic
just to represent the sequence, before any operation is applied. The
`String` representation is one byte per base plus a null terminator.

The cost is that character access goes through `s[i]`, which returns
a fresh 1-character `String` each time. This is fine for reading; for
loops that access every character, it is worth knowing that each
access is an allocation. The library's hot loops (translation,
complement, reverse) all iterate over characters and accumulate into a
single output String; the per-character allocation is real but small,
and the alternative — exposing a `char` type — is a language change,
not a library change.

### Why the codon table is a `Map`

The codon table is consulted once per 3-character window in `translate`
and once per 3-character window in `codon_usage`. A `Map` lookup is
O(1) with a small constant, and the table is built once at program
start. A `switch` statement would be marginally faster but would have
to be written out three times (once in `codon_to_aa`, once in
`is_stop_codon`, once in `codon_for`) and kept in sync.

The table-as-data approach also means you can enumerate it: `keys()` on
`bio_rna_codons` gives you all 64 codons in unspecified order, which is
occasionally useful for building your own indices.

### Why `reverse_translate` appends a stop codon

The output is meant to be translatable, and a translatable mRNA
conventionally ends with a stop. Without the trailing stop, calling
`translate(reverse_translate(p))` on a protein `p` would produce a
protein whose length is `p.length()`, which is fine, but the sequence
would not be a complete coding sequence. Adding the stop makes
`translate(reverse_translate(p))` produce `p + "*"` (with a trailing
stop in the output), which is what a ribosome would produce from that
mRNA.

If you do not want the trailing stop, strip it: the last three
characters of the return value.

### Why the three interfaces are separate types

`DNA`, `RNA`, and `Protein` are structurally identical — two String
fields and two methods. Making them separate interfaces costs almost
nothing and buys one thing: the type system will stop you from passing
an RNA to a function that expects DNA, and vice versa. For a library
where the whole point is to keep track of which alphabet you are
currently in, this is the right trade-off.

If you want untyped sequence code, use plain `String`s. The library's
functions that take `String` (all of them, underneath the wrappers)
accept either.

### Why no mutation

Every operation returns a fresh value. This means `bio.reverse(d)`
produces a new DNA rather than flipping `d` in place. The alternative —
a mutating `reverse_inplace(d)` — would be faster by one allocation
and would be a footgun in any code that shares the DNA value across
call sites. Vyne's default is value semantics for strings, and the
library follows the language.

---

## Related

- `vmath` — scalar math. `vbio` does not currently depend on it, but
  any extension that computes statistics over sequences (entropy,
  information content, position weight matrices) will want it.
- `vcolors` — ANSI color helpers. `vbio` does not use them; the library
  produces no diagnostics of its own beyond returning `"X"`, `"N"`,
  and `"NNN"` for unrecognized input.
- `vmem` — arena checkpoint primitive. Use it to bound memory in any
  loop that calls `vbio` functions repeatedly.
- `ml_seq` — the RNA-sequence classifier in `tests/training/` uses
  `vbio` for codon-usage feature extraction. That program is the
  best existing example of the library in a real workload.

---

## Roadmap

Part 2 of the library is planned and will cover:

- **Protein properties** — molecular weight, isoelectric point,
  hydrophobicity (Kyte-Doolittle), aromaticity.
- **Alignment** — global (Needleman-Wunsch) and local (Smith-Waterman)
  with configurable scoring. Deferred until the crate's scoring
  infrastructure is designed.
- **ORF finding** — six-frame translation, minimum length filtering,
  optional start/stop codon constraints.

Part 3 is speculative and will cover:

- **Motif search** — position weight matrices, consensus sequences.
- **K-mer composition** — the feature extraction used by the
  classifier demo, exposed as a first-class function rather than
  recomputed per caller.
- **Codon optimization** — the inverse of reverse translation, with a
  target host's codon usage table.

None of these are scheduled. The current release is a stable core; new
surface will land as the workloads that need it arrive.

---

## Version history

**0.1.0** — initial release. Types, Codon, Sequence, facade. mRNA
codon table, translation, reverse translation, codon usage, GC
content, melting temperature, complement and reverse complement,
Hamming distance, string search.

```

```
