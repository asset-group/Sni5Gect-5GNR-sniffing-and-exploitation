clear
rm core.*
rm logs/* | true
script -c "./build/shadower/shadower configs/srsran-n78-20MHz-b210.yaml" -O logs/sni5gect.log --flush
# script -c "./build/shadower/shadower configs/srsran-n3-20MHz-b210.yaml" -O logs/sni5gect.log --flush