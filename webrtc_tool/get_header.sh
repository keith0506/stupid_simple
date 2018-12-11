#!/bin/bash
mkdir ../include
src=`find  -name "*.h*"`
echo $src
for obj in $src
do
    echo "cp header file $obj"
    cp --parents $obj ../include
done

