#!/bin/sh

GIT_VER=$(git log|grep ^commit|wc -l|sed -e "s/^ *//")
MD5=$(git log|head -1|awk '{printf $2}')
TMP_FILE=/tmp/version
echo "#define VERSION \"0.9.35.$GIT_VER\"" > ${TMP_FILE}
echo "#define VERSION_MD5 \"$MD5\"" >> ${TMP_FILE}
if [ ! -f $1 ];then
mv -f ${TMP_FILE} $1
else
diff ${TMP_FILE} $1
if [ $? -eq 1 ];then
mv -f ${TMP_FILE} $1
else
rm -f ${TMP_FILE}
fi
fi
