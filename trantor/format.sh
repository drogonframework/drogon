#!/bin/sh

clang-format --version

find trantor -name *.h -o -name *.cc -exec dos2unix {} \;
find trantor -name *.h -o -name *.cc|xargs clang-format -i -style=file
