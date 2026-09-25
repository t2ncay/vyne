# bio — a molecular biology library for Vyne

Part 1: type system, genetic code, sequence primitives.

## Layout

- `Types.vy` — DNA / RNA / Protein interfaces + factories
- `Codon.vy` — codon table, translation, reverse-translation
- `Sequence.vy` — complement, reverse-complement, transcribe, GC, Tm
- `bio.vy` — facade; imports everything above

## Usage

    use "vbio/bio.vy";
    d = bio.make_dna("ATGGCTTAA");
    out(bio.translate(bio.transcribe(d).seq).seq);

## Roadmap

- Part 2: Protein (MW, pI, hydrophobicity), Alignment, ORF
- Part 3: Motif search, k-mers, codon optimization, secondary structure
