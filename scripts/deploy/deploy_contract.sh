
# create account
createAccount() {
    contractAccount=$1
    createAccountScript="cleos wallet create -n ${contractAccount}.wallet --to-console"
    ret=`ssh sh-misc "${remoteDockerScrip} '${createAccountScript}'"`
    echo $ret
    #get priv key 
    privKey=`echo $ret | sed -n '4,4p' | awk '{print substr($1,2)}' | sed 's/.$//'`
    echo "privKey: ${privKey}"
}

# create contract
# param1: file name
createContract() {
    otcFileName=$1
    otcContractName=$2
 
    unlockScript='cleos wallet unlock --password PW5KQzzoYJcijs2wtMpF5Vqk4v8n9FNcxxHj1aqqcjpGJDEkdBrog'
    ssh sh-misc "${remoteDockerScrip} '${unlockScript}'"

    contractFilePath='/opt/mgp/node_devnet/data/otccontract'
    otcFileName='otcbook'
    setContractScript="cleos set contract ${otcContractName} ${contractFilePath} ${otcFileName}.wasm ${otcFileName}.abi -p ${otcContractName}@active"
    ssh sh-misc "${remoteDockerScrip} '${setContractScript}'"
}

accountTail=$1
#build
DOCKER_ID=amax-dev-sean
remoteDockerScrip='docker exec -i mgp-devnet /bin/bash -c'
buildScript='cd /opt/data/build && rm -rf ./* && cmake .. && make -j8'
docker exec -it $DOCKER_ID /bin/bash -c ${buildScript}

#scp
remoteContractPath=/mnt/data/mgp-test/eosio-docker/node_devnet/data/otccontract
scp build/contracts/otcbook/otcbook* sh-misc:${remoteContractPath}
scp build/contracts/otcconf/otcconf* sh-misc:${remoteContractPath}


otcFileName='otcbook'
#create account
otcContractName="${otcFileName}.${accountTail}"
createAccount otcContractName

#deploy contract
createContract ${otcFileName} ${otcContractName}

otcConfName='otcconf'
otcContractName="${otcConfName}.${accountTail}"


#sh-misc target
#/mnt/data/mgp-test/eosio-docker/node_devnet/data/otccontract