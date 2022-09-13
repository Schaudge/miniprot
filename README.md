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
- [Users' Guide](#uguide)
  - [Installation](#install)
  - [Usage](#usage)
  - [Preliminary evaluation](#eval)
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

### <a name="eval"></a>Preliminary evaluation

We aligned 21,919 canonical mouse proteins from Gencode M30 against pre-indexed
GRCh38. It took about 7 minutes over 16 threads. Miniprot mapped 19,308
proteins with 160,119 introns in their best alignment. In comparison to Gencode
v40 human annotations, 93.6% of these introns are annotated in Gencode with
exact coordinates. The global mode of [spaln][spaln] (`-Q7`) mapped 18,907
proteins in 90 min. It identified 185,442 introns with 87.1% of them coincide
with Gencode. Its local mode (`-Q7 -LS`) crashed.

We also aligned 52,089 zebrafish proteins, including alternative isoforms,
against GRCh38. Miniprot mapped 34,288 of them with 81.4% of 298,495 predicted
introns confirmed by the Gencode annotation. The accuracy is lower because
zebrafish is more distant from human and because the input includes rarer
isoforms.

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
