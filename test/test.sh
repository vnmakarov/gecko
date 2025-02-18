#!/bin/sh
#
# Copyright (c) 2025  Vladimir Makarov <vmakarov@gcc.gnu.org>
#
# Usage: test.sh
#

SRCDIR=`dirname $0`

GCC='gcc -O0 -w -g'
outfile=./a.out

if $GCC -I$SRCDIR/.. -I$SRCDIR $SRCDIR/test06.c $SRCDIR/../gecko.c -o $outfile && $outfile 0; then
  echo test06 -- ok
else
  echo test06 -- FAIL
  exit 1
fi

rm -f $outfile
exit 0
