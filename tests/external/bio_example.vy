# bio/example.vy — Part-1 smoke test.

ruleset { dynamic_casting };

use lib "vbio/bio.vy";
use lib "vbio/Types.vy";
use lib "vbio/Codon.vy";
use lib "vbio/Sequence.vy";

d :: bio.Types.DNA = bio.make_dna("ATGGCTTAA");
out("Input:    " + d.seq);
out("Revcomp:  " + bio.reverse_complement(d.seq));

r :: bio.Types.RNA = bio.transcribe(d);
out("mRNA:     " + r.seq);

p :: bio.Types.Protein = bio.translate(r.seq);
out("Protein:  " + p.seq);
out("Length:   " + string(p.length()));

g :: bio.Types.DNA = bio.make_dna("GCGCATAT");
out("GC%:      " + string(bio.gc_content(g.seq) * 100.0));
out("Tm:       " + string(bio.melting_temperature(g.seq)));
out("Find ATG: " + string(bio.find_first(g.seq, "ATG")));

counts :: Map = bio.codon_usage(r.seq);
out("Codons:   " + string(counts));