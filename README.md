# Accurity
Accurity is a computational method that infers tumor purity and tumor cell ploidy from tumor-normal WGS (whole exome will probably work too) data by jointly modelling SCNAs and heterozygous germline single-nucleotide-variants (HGSNVs). Results from both in silico and real sequencing data demonstrated that Accurity is highly accurate and robust, even in low-purity, high-ploidy, and low-coverage (as low as 1X) settings in which several existing methods perform poorly. Accounting for tumor purity and ploidy, Accurity significantly increased the signal/noise gaps between different copy numbers.

Z. Luo*, X. Fan*, Y. Su, YS. Huang (2018). Accurity: Accurate tumor purity and ploidy inference from tumor-normal WGS data by jointly modelling somatic copy number alterations and heterozygous germline single-nucleotide-variants. *Bioinformatics*.

For more information, check https://www.yfish.org/display/PUB/Accurity

To compile, go into Accurity/ and type "make debug" or "make release".