#!/bin/bash
./check_result.sh
dir="records/"$1
if [ ! -d $dir ]
then
	mkdir $dir
else
	rm -rf $dir
	mkdir $dir
fi

cp sender/log/sender.log $dir/send.logx
cp receiver/log/receiver.log $dir/rece.logx
cp sender/.run $dir/.senderrun
cp receiver/.run $dir/.receiverrun
