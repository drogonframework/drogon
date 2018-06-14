#!/bin/sh
echo "update config.h"
if [ ! -f $2 ];then
mv -f $1 $2
else
diff $1 $2
if [ $? -eq 1 ];then
mv -f $1 $2
fi
fi
rm -f $1
