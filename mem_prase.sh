#!/bin/bash

elf=$1
files=$2

########################################################################
# objdump -d x.elf > elf.txt   |   s32k144 dump mem.txt
########################################################################
[ -z "$elf" -o -z "$files" ] && {
    echo "${0:2} [elf files] [mem out files]"
    exit 1
}

[ ! -e "$elf" -o ! -e "$files" ] && {
    echo "${0:2} [$elf/$files] No such file or directory"
    exit 2
}

########################################################################
# $1 is elf objdump files, $2 is malloc point
# this functions return the malloc lr
########################################################################
function search_alloc_name(){
    cat $1 | grep "<*>:" | awk 'BEGIN {
        name="test";
    }{
        addr=strtonum("0x"$1);
        com=strtonum(v);
        #print "com=",com , "addr=",addr, "name=",name
        if(com > addr){
            name=$2
        }
    } END {
        print name
    }' v=$2
}

########################################################################
# $1 is oom files,$2 is the same malloc point address
# this functions return total malloc size
########################################################################
function calc_alloc_size(){
    cat $1 | grep $2 | awk 'BEGIN{total=0} {total = total + $6} END{printf total}'
}

########################################################################
# $1 is oom files
# this functions return total malloc size
########################################################################
function calc_alloc_total_size(){
    cat $1 | awk 'BEGIN{total=0} {total = total + $6} END{printf total}'
}

########################################################################
# $1 is oom files
# this functions return alloc point list
########################################################################
function search_alloc_point(){
    cat $1 | awk '{print $4}' | sort | uniq
}

function main(){
    alloc_point_list=$(search_alloc_point $files)
    for point in $alloc_point_list;do
        function_name=$(search_alloc_name $elf $point)
        echo -e "\033[32m${function_name//[<>:]/}\033[0m"
        echo -e "\t\033[44;37m$point\033[0m cost \033[31m$(calc_alloc_size $files $point)\033[0m bytes."
    done

    echo
    echo -e "Total memory: \033[41;33m$(calc_alloc_total_size $files)\033[0m bytes."
}

main
