# $1: domain size
# $2: tag domain size
# $3: maximum dependency sets per tag
# $4: tag probability
# $5: dependency probability
# $6: maximum constraints
# $7: constraint probability
# $8: constraint element probability

# domain
for (( d=1; d <= $1; d++ ))
do
	echo -n "domain(d$d). "
done
echo ""

# requirements
propReq=$((32768 * $4 / 100))
propDep=$((32768 * $5 / 100))
propCons=$((32768 * $7 / 100))
propConsElem=$((32768 * $8 / 100))
for (( t=1; t<=$2; t++ ))
do
	# another requirement?
	for (( rc=1; rc <= $3; rc++ ))
	do
		if [[ $RANDOM -le $propReq ]]; then
			echo -n "tagreq(t$rc,p"
			for ((d=1; d<=$1; d++ ))
			do
				if [[ $RANDOM -le $propDep ]]; then 
					echo -n ",d$d"
				fi
			done
			echo -n ",n"
			for ((d=1; d<=$1; d++ ))
			do
				if [[ $RANDOM -le $propDep ]]; then 
					echo -n ",d$d"
				fi
			done
			echo "). "
		fi
	done
done

# another constraint?
for (( rc=1; rc <= $6; rc++ ))
do
	if [[ $RANDOM -le $propCons ]]; then
		first=1
		for ((t=1; t<=$1; t++ ))
		do
#			if [[ $RANDOM -le 16384 ]]; then
#				naf=" not"
#			else
#				naf=""
#			fi
			if [[ $RANDOM -le $propConsElem ]]; then
				if [[ $first == 1 ]]; then
					echo -n ":-$naf tags(t$t)"
					first=0
				else
					echo -n ",$naf tags(t$t)"
				fi
			fi
		done
		if [[ $first == 0 ]]; then
			echo ". "
		fi
	fi
done

