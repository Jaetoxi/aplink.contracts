
#scp
scpToMiscMerchine(){
    remoteContractPath=/mnt/data/mgp-test/eosio-docker/node_devnet/data/otccontract
    scp build/contracts/otcbook/otcbook* sh-misc:${remoteContractPath}
    scp build/contracts/otcconf/otcconf* sh-misc:${remoteContractPath}
    scp scripts/deploy/60-remote-initdata.sh sh-misc:${remoteContractPath}

}
scpToMiscMerchine