#!/usr/bin/env bash

clang-format --version
find lib orm_lib nosql_lib examples drogon_ctl -name *.h -o -name *.cc -exec dos2unix {} \;
find lib orm_lib nosql_lib examples drogon_ctl -name *.h -o -name *.cc|xargs clang-format -i -style=file
