#!/bin/sh

GIT_VER=$(git log|grep ^commit|wc -l|sed -e "s/^ *//")
MD5=$(git log|head -1|awk '{printf $2}')
TMP_FILE=/tmp/version
echo "#define MAJOR $2" > ${TMP_FILE}
echo "#define MINOR $3" >> ${TMP_FILE}
echo "#define PATCH $4" >> ${TMP_FILE}
if [ $# -gt 4 ];then
echo "#define VERSION \"$2.$3.$4.$5.$GIT_VER\"" >> ${TMP_FILE}
else
echo "#define VERSION \"$2.$3.$4.$GIT_VER\"" >> ${TMP_FILE}
fi
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
