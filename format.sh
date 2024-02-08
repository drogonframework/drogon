#!/usr/bin/env bash

# You can customize the clang-format path by setting the CLANG_FORMAT environment variable
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}

# Check if clang-format version is 17 to avoid inconsistent formatting
$CLANG_FORMAT --version
if [[ ! $($CLANG_FORMAT --version) =~ "version 17" ]]; then
    echo "Error: clang-format version must be 17"
    exit 1
fi

find lib orm_lib nosql_lib examples drogon_ctl -name *.h -o -name *.cc -exec dos2unix {} \;
find lib orm_lib nosql_lib examples drogon_ctl -name *.h -o -name *.cc|xargs $CLANG_FORMAT -i -style=file
