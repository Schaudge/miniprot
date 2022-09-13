## <a name="started"></a>Getting Started
```sh
# download and compile
git clone https://github.com/lh3/miniprot
cd miniprot && make

# test file
./miniprot test/DPP3-hs.gen.fa.gz test/DPP3-mm.pep.fa.gz > aln.paf        # PAF output
./miniprot --gff test/DPP3-hs.gen.fa.gz test/DPP3-mm.pep.fa.gz > aln.gff  # GFF3+PAF output

# general command line
./miniprot -t16 -d genome.mpi genome.fna                 # indexing optional but recommended
./miniprot -ut16 --gff genome.mpi protein.faa > aln.gff  # alignment

# output format
man ./miniprot.1
```

## Table of Contents

- [Getting Started](#started)
- [Introduction](#intro)
- [Users' Guide](#uguide)
  - [Installation](#install)
  - [Usage](#usage)
  - [Evaluation](#eval)
  - [Algorithm overview](#algo)
- [Limitations](#limit)

## <a name="intro"></a>Introduction

Miniprot aligns a protein sequence against a genome with affine gap penalty,
splicing and frameshift. It is primarily intended for annotating protein-coding
genes in a new species using known genes from other species. Miniprot is
similar to [GeneWise][genewise] and [Exonerate][exonerate] in functionality but
it can map proteins to whole genomes and is much faster at the residue
alignment step.

Miniprot is not optimized for mapping distant homologs because distant homologs
are less informative to gene annotations. Nonetheless, it is still possible to
tune seeding parameters to achieve higher sensitivity at the cost of
performance.

## <a name="uguide"></a>Users' Guide

### <a name="install"></a>Installation

Miniprot requires SSE2 or NEON instructions and only works on x86\_64 or ARM
CPUs. It depends on [zlib][zlib] for parsing gzip'd input files. To compile
miniprot, type `make` in the source code directory. This will produce a
standalone executable `miniprot`. This executable is all you need to invoke
miniprot.

### <a name="usage"></a>Usage

To run miniprot, use
```sh
miniprot -t8 ref-file protein.faa > output.paf
```
where `ref-file` can either be a genome in the FASTA format or a pre-built
index generated by
```sh
miniprot -t8 -d ref.mpi ref.fna
```
Because miniprot indexing is slow and memory intensive, it is recommended to
pre-build the index. FASTA input files can be optionally compressed with gzip.

Miniprot outputs alignment in the protein PAF format. Different from the more
common nucleotide PAF format, miniprot uses more CIGAR operators to encode
introns and frameshifts. Please refer to the manpage for detailed explanation.

The PAF format gives full alignment information. For convenience, miniprot can
also output GFF3 with option `--gff`:
```sh
miniprot -t8 --gff -d ref.mpi ref.fna > out.gff
```
The detailed alignment is embedded in `##PAF` lines in the GFF3 output.

### <a name="eval"></a>Evaluation

We collected Ensembl canonical mouse proteins from Gencode vM30 and longest
proteins per gene for chicken and zebrafish. We then aligned these proteins to
the human reference genome GRCh38. We say a junction is confirmed if it can be
found in the human Gencode annotation v41; a junction is non-overlapping if the
intron in the junction does not overlap with any introns in the Gencode
annotation.

We only evaluated miniprot and [spaln][spaln] as these are the only tools
practical for whole genomes. In addition, [Iwata and Gotoh (2012)][spaln2]
suggest that spaln2 consistently outperforms exonerate, GeneWise, ProSplign and
genBlastG.

In the evaluation, both miniprot and spaln were set to use 16 CPU threads. We
used option `-Q7 -O0 -Thomosapi` with spaln. This does global alignment with
the human-specific splice model.

|Metric          |mouse/mp |mouse/sp |chicken/mp|zebrafish/mp|
|:---------------|--------:|--------:|--------:|--------:|
|Elapsed time (s)|     347 |   3,714 |     294 |     464 |
|# proteins      |  21,844 |  21,844 |  17,007 |  30,313 |
|# mapped        |  19,253 |  18,847 |  13,284 |  19,797 |
|# single-exon   |   2,878 |         |   1,110 |   1,857 |
|# predicted junc| 164,718 | 173,475 | 131,346 | 176,044 |
|# non-ovlp junc |     402 |   1,490 |     482 |     960 |
|# confirmed junc| 157,400 | 162,303 | 117,416 | 151,912 |
|% confirmed     |    95.6 |    93.6 |    89.4 |    86.3 |

On the human-mouse dataset, miniprot finds fewer novel splice junctions,
implying higher specificity, but spaln finds more confirmed junctions, implying
higher sensitivity. This is partly because spaln forces global alignment.
I have tried a few other options of spaln such as `-yS`, `-M` (more than one
hits per query) and `-LS` (local mode), but spaln-2.4.12 crashed. Not using a
species-specific splice model (`-T`) would lead to lower accuracy.

### <a name="algo"></a>Algorithm overview

1. Translate the reference genome to amino acids in six phases and filter out
   ORFs shorter than 45bp. Reduce 20 amino acids to 13 distinct integers and
   extract random open syncmers of 6aa in length. By default, miniprot selects
   20% of 6-mers in average. For a reduced 6-mer at reference position `x`,
   keep the 6-mer and `floor(x/256)` in a dense hash table. This concludes the
   indexing step.

2. Given a protein sequence as query, extract 6-mer syncmers on the protein,
   look up the index for seed matches and apply minimap2-like chaining. This
   first round of chaining is approximate as the reference positions have been
   binned during indexing.

3. For each chain in step 2, redo seeding and chaining with sliding 5-mers from
   both the reference and the protein in the original chain. Miniprot uses all
   reduced 5-mers for this second round of chaining.

4. Choose top 100 (see `-N`) chains. Filter out anchors around potential
   introns or long gaps. Perform striped dynamic programming between remaining
   anchors and also extend from the first or last anchors. This gives the final
   alignment.

## <a name="limit"></a>Limitations

* The initial conditions of dynamic programming are not technically correct,
  which may result in suboptimal residue alignment in rare cases.

* Support for non-splicing alignment needs to be improved.

* More manual inspection required for improved accuracy. For example, tandem
  copies in segmental duplications could be handled more carefully.

[exonerate]: https://pubmed.ncbi.nlm.nih.gov/15713233/
[genewise]: https://pubmed.ncbi.nlm.nih.gov/15123596/
[zlib]: https://zlib.net
[paftools]: https://github.com/lh3/minimap2/blob/master/misc/paftools.js
[minimap2]: https://github.com/lh3/minimap2
[spaln]: https://github.com/ogotoh/spaln
[spaln2]: https://pubmed.ncbi.nlm.nih.gov/22848105/
