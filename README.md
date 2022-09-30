[![Release](https://img.shields.io/github/v/release/lh3/miniprot?include_prereleases)](https://github.com/lh3/miniprot/releases)
[![BioConda Install](https://img.shields.io/conda/dn/bioconda/miniprot.svg?style=flag&label=BioConda%20install)](https://anaconda.org/bioconda/miniprot)
[![Build Status](https://github.com/lh3/miniprot/actions/workflows/ci.yaml/badge.svg)](https://github.com/lh3/miniprot/actions)
## <a name="started"></a>Getting Started
```sh
# download and compile
git clone https://github.com/lh3/miniprot
cd miniprot && make

# test file
./miniprot test/DPP3-hs.gen.fa.gz test/DPP3-mm.pep.fa.gz > aln.paf        # PAF output
./miniprot --gff test/DPP3-hs.gen.fa.gz test/DPP3-mm.pep.fa.gz > aln.gff  # GFF3+PAF output

# general command line: index and align in one go
./miniprot -ut16 --gff genome.fna protein.faa > aln.gff

# general command line: index first and then align (recommended)
./miniprot -t16 -d genome.mpi genome.fna
./miniprot -ut16 --gff genome.mpi protein.faa > aln.gff

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
introns and frameshifts. Please refer to the [manpage][manpage] for detailed explanation.

For convenience, miniprot can also output GFF3 with option `--gff`:
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

We only evaluated miniprot v0.3 and [spaln][spaln] v2.4.13a as these are the
only tools practical for whole genomes. Running other tools would require to
find approximate protein mapping first and then realign in each local region.
This procedure is complex and does not evaluate the mapping step. In addition,
[Iwata and Gotoh (2012)][spaln2] suggest that spaln2 consistently outperforms
exonerate, GeneWise, ProSplign and genBlastG.

In the evaluation, both miniprot (mp) and spaln (sp) were set to use 16 CPU
threads. We used option `-Q7 -O0 -Thomosapi -LS -yS` with spaln (local
alignment with the human-specific splicing model). It gives the best accuracy
for the mouse dataset. The same setting crashed on the chicken and the
zebrafish datasets for the moment.

|Metric          |mouse/mp |mouse/sp |chicken/mp|zebrafish/mp|
|:---------------|--------:|--------:|--------:|--------:|
|Elapsed time (s)|     367 |   3,767 |     318 |     494 |
|Peak RAM (Gb)   |    15.1 |     5.6 |    14.0 |    18.2 |
|# proteins      |  21,844 |  21,844 |  17,007 |  30,313 |
|# mapped        |  19,375 |  18,840 |  13,496 |  20,382 |
|# single-exon   |   2,960 |         |   1,253 |   2,098 |
|# predicted junc| 165,219 | 171,241 | 132,275 | 179,961 |
|# non-ovlp junc |     377 |     852 |     435 |     800 |
|# confirmed junc| 158,164 | 162,551 | 119,492 | 157,004 |
|% confirmed     |    95.7 |    94.9 |    90.3 |    87.2 |

On the human-mouse dataset, miniprot finds fewer novel splice junctions,
implying higher specificity, but spaln finds more confirmed junctions, implying
higher sensitivity.

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
[manpage]: https://lh3.github.io/miniprot/miniprot.html
