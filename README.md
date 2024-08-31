The repository is a modified version of the [HipSTR](https://github.com/HipSTR-Tool/HipSTR) tool, which was initially designed for genotyping Short Tandem Repeats (STRs) using Illumina sequencing data. The code of HipSTR has been tailored to enhance its performance specifically for long reads (PacBio HiFi and Oxford Nanopore) to genotype both STRs and VNTRs. The documentation presented below predominantly derives from the original documentation, with certain sections having been either added or omitted.


# LongTR

## Requirements 
gcc 7+

cmake 3.12+

LongTR relies on [spoa](https://github.com/rvaser/spoa) and [HTSLIB](https://github.com/samtools/htslib). These dependencies will be automatically downloaded and installed during the setup process. To ensure a successful installation, please ensure that the following dependencies are already installed on your system:

[google/googletest 1.10.0+](https://github.com/google/googletest)

[zlib](https://zlib.net/)

## Installation
To obtain LongTR, use:

	git clone git@github.com:gymrek-lab/LongTR.git

To build, use Make:

    cd LongTR
    make

The commands will construct an executable file called **LongTR** in the current directory, for which you can view detailed help information by typing 

	./LongTR --help

## Quick Start
When executing the tool on a PacBio HiFi dataset, the following parameters are recommended:

```
./LongTR --bams             long_read_sample1.bam,long_read_sample2.bam,...
         --fasta            genome.fa
         --regions          tr_regions.bed
         --tr-vcf           tr_calls.vcf.gz
	 --phased-bam
```

* **--bams** :  a comma-separated list of [BAM/CRAM](#bams) files generated by [BWA-MEM](http://bio-bwa.sourceforge.net/bwa.shtml) and sorted and indexed using [samtools](http://www.htslib.org/)
* **--fasta** : [FASTA file](https://en.wikipedia.org/wiki/FASTA_format) containing the sequence for each chromosome in the BED file. This build's coordinates must match those of the TR regions
* **--regions** : a [BED](#str-bed) file containing the 1-based coordinates for each TR region of interest
* **--tr-vcf** : The output path for the TR genotypes
* **--phased-bam** : For using haplotype-specific bam files. Input bam file should be happlotagged first to use this option

  
Additional parameters:

* **--stutter-align-len**: Switch to the alignment with error modeling, only for homopolymers with a length shorter than the specified threshold
* **--min-mapq**: Filter reads with MAPQ less than a provided threshold (Default 20)
* **--indel-flank-len <max_bp>**: Include InDels in max_bp base pair around repeat as part of the repeat (Default = 5)
* **--min-mean-qual** : Threshold for average quality of read which is based on Illumina 1.8 Phred+33 quality score system (Default 30)
* **--max-tr-len** : Maximum length of TR that will be genotyped by LongTR (Default 1000)
* **--min-reads** : Minimum total reads required to genotype a locus (Default = 10)
* **--alignment-params a,b,c,d,e,f,g**: Custom alignment parameters. Numbers show log probabilities

Parameter | Transition
--- | -----------
a   | Insertion to insertion (Default = -1.0)
b   | Insertion to match (Default = -0.458675)
c   | Deletion to deletion (Default = -1.0)
d   | Deletion to match (Default =  -0.458675)
e   | Match to match (Default = -0.00005800168)
f   | Match to insertion (Default = -10.448214728)
g   | Match to deletion (Default = -10.448214728)

The default settings are optimized for high-quality reads with minimal InDels, such as PacBio HiFi reads. For reads with higher error rates, consider increasing the gap-opening log probabilities (f and g) to better accommodate the occurrence of InDels.

For each region in *tr_regions.bed*, **LongTR** will output the resulting TR genotypes to *tr_calls.vcf.gz*, a [bgzipped](http://www.htslib.org/doc/tabix.html) [VCF](#str-vcf) file. This VCF will contain calls for each sample in any of the BAM/CRAM files' read groups.


## Speed
LongTR doesn't currently have multi-threaded support, but there are several options available to accelerate analyses:

1. Analyze each chromosome in parallel using the **--chrom** option. For example, **--chrom chr2** will only genotype BED regions on chr2
2. Split your BED file into *N* files and analyze each of the *N* files in parallel. This allows you to parallelize analyses in a manner similar to option 1 but can be used for increased speed if *N* is much greater than the number of chromosomes.


## Call Filtering
It's important to filter the resulting VCFs to discard low-quality calls. To facilitate this process, the VCF output contains various FORMAT and INFO fields that are usually indicators of problematic calls. The INFO fields indicate the aggregate data for a locus and, if certain flags are raised, may suggest that the entire locus should be discarded. In contrast, FORMAT fields are available on a per-sample basis for each locus and, if certain flags are raised, suggest that some samples' genotypes should be discarded. The list below includes some of these fields and how they can be informative:

#### INFO fields:  
1. **DP**: Reports the total depth/number of informative reads for all samples at the locus. The mean coverage per-sample can obtained by dividing this value by the number of samples with non-missing genotypes. In general, genotypes with a low mean coverage are unreliable because the reads may only have captured one of the two alleles if an individual is heterozygous.
2. **DFLANKINDEL**: Reports the total number of reads for which the maximum likelihood alignment contains an indel in the regions flanking the TR. A high fraction of reads with this artifact (DFLANKINDEL/DP) can be caused by an actual indel in a region neighboring the TR. However, it can also arise if LongTR fails to identify sufficient candidate alleles. When these alleles are very different in size from the candidate alleles or are non-unit multiples, they're frequently aligned as indels in the flanking sequences.

#### FORMAT fields:  
1. **Q**: Reports the posterior probability of the genotype. We've found that this is the best indicator of the quality of an individual sample's genotype and almost always use it to filter calls.   
2. **DP** and **DFLANKINDEL**: Identical to the INFO field case, these fields are also available for each sample and can be used in the same way to identify problematic individual calls.  


## Additional Usage Options

| Option  | Description  
| :------- | :----------- 
| **log**           log.txt               | Output the log information to the provided file (Default = Standard error)  
| **haploid-chrs**  list_of_chroms      | Comma separated list of chromosomes to treat as haploid (Default = all diploid) <br> **Why? You're analyzing a haploid chromosome like chrY**  
| **bam-samps**     list_of_read_groups | Comma separated list of samples in same order as BAM files. <br> Assign each read the sample corresponding to its file. By default, <br> each read must have an RG tag and and the sample is determined from the SM field <br> **Why? Your BAM file RG tags don't have an SM field**  
| **bam-libs**      list_of_read_groups | Comma separated list of libraries in same order as BAM files. <br> Assign each read the library corresponding to its file. By default, <br> each read must have an RG tag and and the library is determined from the LB field <br> NOTE: This option is required when --bam-samps has been specified <br> **Why? Your BAM file RG tags don't have an LB tag**  
|**output-filters**                        | Write why individual calls were filtered to the VCF (Default = False)


This list is comprised of the most useful and frequently used additional options, but is not all encompassing. For a complete list of options, please type

	./LongTR --help

<a id="aln-viz"></a>

## File Formats
<a id="bams"></a>

### BAM/CRAM files
LongTR requires [BAM/CRAM](https://samtools.github.io/hts-specs/SAMv1.pdf) files produced by any indel-sensitive aligner. These files must have been sorted by position using the `samtools sort` command and then indexed using `samtools index`. To associate a read with its sample of interest, LongTR uses read group information in the BAM/CRAM header lines. These *@*RG lines must contain an *ID* field, an *LB* field indicating the library and an *SM* field indicating the sample. For example, if a BAM/CRAM contained the following header line

	@RG     ID:RUN1 LB:ERR12345        SM:SAMPLE789

an alignment with the RG tag 

	RG:Z:RUN1

will be associated with sample *SAMPLE789* and library *ERR12345*. In this manner, LongTR can analyze BAMs/CRAMs containing more than one sample and/or more than one library and can handle cases in which a single sample's reads are spread across multiple files.

Alternatively, if your BAM/CRAM files lack *RG* information, you can use the **bam-samps** and **bam-libs** flags to specify the sample and library associated with each file. In this setting, however, a BAM/CRAM can only contain a single library and a single read group. For example, the command

```
./LongTR --bams             run1.bam,run2.bam,run3.bam,run4.cram
         --fasta            genome.fa
         --regions          tr_regions.bed
         --tr-vcf           tr_calls.vcf.gz
         --bam-samps        SAMPLE1,SAMPLE1,SAMPLE2,SAMPLE3
         --bam-libs         LIB1,LIB2,LIB3,LIB4
```

essentially tells LongTR to associate all the reads in the first two BAMS with *SAMPLE1*, all the reads in the third file with *SAMPLE2* and all the reads in the last BAM with *SAMPLE3*.


LongTR can analyze both BAM and CRAM files simultaneously, so if your project contains a mixture of these two file types, LongTR will automatically perform CRAM decompression as necessary. **When analyzing CRAM files, please ensure that the file provided to --fasta is the same FASTA file used during CRAM generation**. Otherwise, CRAM decompression will likely fail and bizarre behavior may occur.

<a id="str-bed"></a>

### TR region BED file
The BED file containing each TR region of interest is a tab-delimited file comprised of 5 required columns and one optional column: 

1. The name of the chromosome on which the TR is located
2. The 1-based start position of the TR on its chromosome
3. The end position of the TR on its chromosome
4. The motif length (i.e. the number of bases in the repeat unit)
5. The number of copies of the repeat unit in the reference allele

The 6th column is optional and contains the name of the TR locus, which will be written to the ID column in the VCF. 
Below is an example file which contains 5 TR loci 

**NOTE: The table header is for descriptive purposes. The BED file should not have a header**

CHROM | START       | END         | MOTIF_LEN | NUM_COPIES | NAME
----  | ----        | ----        | ---       | ---        | ---
chr1  | 13784267    | 13784306    | 4         | 10         | GATA27E01
chr1  | 18789523    | 18789555    | 3         | 11         | ATA008
chr2  | 32079410    | 32079469    | 4         | 15         | AGAT117
chr17 | 38994441    | 38994492    | 4         | 12         | GATA25A04
chr17 | 55299940    | 55299992    | 4         | 13         | AAT245


<a id="str-vcf"></a>

### VCF file
For more information on the VCF file format, please see the [VCF spec](http://samtools.github.io/hts-specs/VCFv4.2.pdf). 

#### INFO fields
INFO fields contains aggregated statistics about each genotyped TR in the VCF. The INFO fields reported by LongTR primarily describe the TR's reference coordinates (START and END) and information about the allele counts (AC) and number of reads used to genotype all samples (DP).

FIELD | DESCRIPTION
----- | -----------
BPDIFFS        | Base pair difference of each alternate allele from the reference allele
START          | Inclusive start coodinate for the repetitive portion of the reference allele
END            | Inclusive end coordinate for the repetitive portion of the reference allele
PERIOD         | Length of TR motif
AN             | Total number of alleles in called genotypes
REFAC          | Reference allele count
AC             | Alternate allele counts
NSKIP          | Number of samples not genotyped due to various issues
NFILT          | Number of samples that were originally genotyped but have since been filtered
INEXACT_ALLELE | Boolean showing if each alternate allele is exact or approximated by POA, 0 for exact 1 for approximated 
DP             | Total number of reads used to genotype all samples
DSNP           | Total number of reads with SNP information
DFLANKINDEL    | Total number of reads with an indel in the regions flanking the TR

#### FORMAT fields
FORMAT fields contain information about the genotype for each sample at the locus. In addition to the most probable phased genotype (GT), LongTR reports information about the posterior likelihood of this genotype (PQ) and its unphased analog (Q). Other useful information reported are the number of reads that were used to determine the genotype (DP) and whether these had any alignment artifacts (DFLANKINDEL).

FIELD     | DESCRIPTION
--------- | -----------
GT        | Genotype
GB        | Base pair differences of genotype from reference
Q         | Posterior probability of unphased genotype
PQ        | Posterior probability of phased genotype
DP        | Number of valid reads used for sample's genotype
DSNP      | Number of reads with SNP phasing information
PDP       | Fractional reads originating from each haplotype, based on haplotag information
GLDIFF    | Difference in likelihood between the reported and next best genotypes
DSNP      | Total number of reads with SNP information
PSNP      | Number of reads with SNPs supporting each haploid genotype
DFLANKINDEL | Number of reads with an indel in the regions flanking the TR
AB        | log10 of the allele bias pvalue, where 0 is no bias and more negative values are increasingly biased. 0 for all homozygous genotypes
FS        | log10 of the strand bias pvalue from Fisher's exact test, where 0 is no bias and more negative values are increasingly biased. 0 for all homozygous genotypes
DAB       | Number of reads used in the allele bias calculation
ALLREADS  | Base pair difference observed in each read's Needleman-Wunsch alignment
MALLREADS | Maximum likelihood bp diff in each read based on haplotype alignments
GL        | log-10 genotype likelihoods
PL        | Phred-scaled genotype likelihoods

## Citation

Please cite our [manuscript](https://doi.org/10.1186/s13059-024-03319-2) when using LongTR for your publications.
