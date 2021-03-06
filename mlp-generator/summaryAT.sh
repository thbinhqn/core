#!/bin/bash

#
# file   summaryAT.sh
# author Tri Kurniawan Wijaya
# date   Tue 26 Apr 2011 01:05:21 PM CEST 
#
# brief: From stats-* create a summary, one file for each type of data: ctrAS, 
#        moduleInstantiation, sizeM, ctrASFromDLV, callToDLV, Time
#	 
# require 2 params
# 1st param: directory target, example: StatsCore
#	     Expected: StatsCore/Module-xxx inside 
# 2nd param: directory result, example: SummaryCore
#


targetDir=$1		#main directory where all folder parameter setting are
resultDir=$2

go=1
if [ -a $resultDir ]; then
  go=0
  read -p "$resultDir is already exist. Do you want to delete it anyway? [y]es / [c]ancel execution: " inp
  if [ "$inp" = "y" ]; then
    rm -rf $resultDir
    go=1
  fi 
fi

if [ $go -eq 1 ]; then

mkdir $resultDir
for mainDir in $targetDir/*; do
  if [ -d $mainDir ]; then
    shortMainDir=${mainDir#$targetDir/}
    mkdir $resultDir/$shortMainDir
    for dir in $mainDir/*; do 
	if [ -d $dir ]; then
		shortDir=${dir#$mainDir/}
		mkdir $resultDir/$shortMainDir/$shortDir
		fileSummaryMI="$resultDir/$shortMainDir/$shortDir/summary-MI-$shortDir.txt"
		fileSummarySizeM="$resultDir/$shortMainDir/$shortDir/summary-SizeM-$shortDir.txt"
		fileSummaryASDLV="$resultDir/$shortMainDir/$shortDir/summary-ASDLV-$shortDir.txt"
		fileSummaryCallDLV="$resultDir/$shortMainDir/$shortDir/summary-CallDLV-$shortDir.txt"
		fileSummaryTime="$resultDir/$shortMainDir/$shortDir/summary-Time-$shortDir.txt"
		fileSummaryCtrAS="$resultDir/$shortMainDir/$shortDir/summary-CtrAS-$shortDir.txt"
		fileSummaryTimeOut="$resultDir/$shortMainDir/$shortDir/summary-TimeOut-$shortDir.txt"
		fileSummaryMemOut="$resultDir/$shortMainDir/$shortDir/summary-MemOut-$shortDir.txt"
		for i in $dir/stats-*; do 
			ctr=-1
			echo "process: $i"
			while read LINE	
			do
				if [ "$LINE" = "timelimit: sending warning signal 1" ]; then
					echo "time-out $i" >> $fileSummaryTimeOut
				elif [ "$LINE" = "fork: Cannot allocate memory" ]; then
					echo "mem-out $i" >> $fileSummaryMemOut
				elif [ "$LINE" = "terminate called after throwing an instance of 'std::bad_alloc'" ]; then
					echo "mem-out $i" >> $fileSummaryMemOut
				elif [ $ctr -ge 0 ]; then
					let data=$ctr%7
					if [ $data -eq 1 ]; then
						lastCtrAS=$LINE
					elif [ $data -eq 2 ]; then
						lastMI=$LINE
					elif [ $data -eq 3 ]; then
						lastSizeM=$LINE
					elif [ $data -eq 4 ]; then
						lastASDLV=$LINE
					elif [ $data -eq 5 ]; then
						lastCallDLV=$LINE
					elif [ $data -eq 6 ]; then
						lastTime=$LINE
					fi
				fi
				let ctr=$ctr+1
			done < $i
			if [ $ctr -ge 6 ]; then
				echo $lastCtrAS >> $fileSummaryCtrAS
				echo $lastMI >> $fileSummaryMI
				echo $lastSizeM >> $fileSummarySizeM
				echo $lastASDLV >> $fileSummaryASDLV
				echo $lastCallDLV >> $fileSummaryCallDLV
				echo $lastTime >> $fileSummaryTime
			fi
		done
	fi  
    done
  fi
done

fi
