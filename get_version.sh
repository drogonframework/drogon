#!/bin/sh

GIT_VER=$(git log|grep ^commit|wc -l|sed -e "s/^ *//")
MD5=$(git log|head -1|awk '{printf $2}')
MAJOR=1
MINOR=0
PATCH=0
PRE_RELEASE_STRING="beta5"
TMP_FILE=/tmp/version
echo "#define MAJOR ${MAJOR}" > ${TMP_FILE}
echo "#define MINOR ${MINOR}" >> ${TMP_FILE}
echo "#define PATCH ${PATCH}" >> ${TMP_FILE}
echo "#define PRE_RELEASE_STRING \"${PRE_RELEASE_STRING}\"" >> ${TMP_FILE}
echo "#define VERSION \"${MAJOR}.${MINOR}.${PATCH}.${PRE_RELEASE_STRING}.$GIT_VER\"" >> ${TMP_FILE}
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
