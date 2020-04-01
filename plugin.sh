#!/bin/bash

if [ -z "$1" ]; then 
	echo "command not provided (add|remove)"
	exit -1
fi

if [ $1 == "add" ]; then 
    if [ -z "$2" ]; then
        echo "plugin name not provided (plugin.sh add [name] [git-url])"
        exit -1
    fi

    if [ -z "$3" ]; then
		echo "plugin git path not provided (plugin.sh add [name] [git-url])"
		exit -1
    fi

    echo "$2" >> plugins
    sort -u plugins -o plugins
	
	git submodule add $3 src/$2
	git submodule init
	
    exit 0
fi

if [ $1 == "remove" ]; then 
    if [ -z "$2" ]; then
        echo "plugin name not provided (plugin.sh remove [name])"
        exit -1
    fi
	
	truncate -s 0 plugins.tmp
	
	while read -r line
	do 
		if [ "$2" != "$line" ]; then 
			echo $line
			echo $line >> plugins.tmp
		fi
	done < plugins
	mv plugins.tmp plugins
	
	git submodule deinit -f -- src/$2
	rm -rf .git/modules/src/$2
	git rm -f src/$2
	
	exit 0
fi

echo "invalid command (add|remove)"
exit -1