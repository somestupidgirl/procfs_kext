#!/bin/bash

count=0
crash=0
error=0

if [[ -e "ft_otool" ]];
then
	printf "Starting binary diff ...\n"
	for entry in "/bin/"*
	do
		count=$((count + 1))
		err=$(./ft_otool $1 $entry > file1)
		$(otool $1 $entry > file2 2>&-)
		diff=$(diff file1 file2)
		if [ -z "$diff" ]
		then
			success=$((success + 1))
			printf "%-40s\033[32m%40s\033[0m\n" $entry "OK"
		else
			error=$((error + 1))
			printf "%-40s\033[31m%40s\033[0m\n" $entry "NO"
		fi
		rm file1 file2
	done
	countlib=1
	printf "Starting lib diff ...\n"
	for entry in "/usr/lib/"*.dylib
	do
		count=$((count + 1))
		countlib=$((countlib + 1))
		err=$(./ft_otool $1 $entry > file1)
		$(otool $1 $entry > file2 2>&-)
		diff=$(diff file1 file2)
		if [ -z "$diff"]
		then
			success=$((sucess + 1))
			printf "%-40s\033[32m%40s\033[0m\n" $entry "OK"
		else
			error=$((error + 1))
			printf "%-40s\033[31m%40s\033[0m\n" $entry "NO"
		fi
		if (( countlib > 5))
		then
			break
		fi
		rm file1 file2
	done
	rm file1 file2
else
	printf "./ft_otool doesnt exist\n"
fi
