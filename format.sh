#!/bin/sh

find lib orm_lib -name *.h -o -name *.cc|xargs clang-format -i -style=file
