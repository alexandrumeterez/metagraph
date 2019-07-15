#!/bin/bash

set -e

basedir=/cluster/work/grlab/projects/metagenome/data
metagraph=/cluster/home/akahles/git/projects/2014/metagenome/metagraph/build/metagraph
K=41
mem=50000
threads=60

echo "/usr/bin/time -v $metagraph transform_anno -v -o ${basedir}/gtex/output_k${K}_merged.anno --anno-type brwt --greedy ${basedir}/gtex/output_k${K}_merged.anno.column.annodbg -p $threads 2>&1" #| bsub -J convert_metasub_to_brwt_pm -oo conversion_to_brwt.k${K}.lsf -W 150:00 -n $threads -R "rusage[mem=${mem}]" -R "span[hosts=1]" 
/usr/bin/time -v $metagraph transform_anno -v -o ${basedir}/gtex/output_k${K}_merged.anno --anno-type brwt --greedy ${basedir}/gtex/output_k${K}_merged.anno.column.annodbg -p $threads > conversion_to_brwt.k${K}.lsf 2>&1 