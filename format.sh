#!/bin/sh

find lib orm_lib examples drogon_ctl -name *.h -o -name *.cc|xargs clang-format -i -style=file
