#!/bin/sh
#
# Copyright (c) 2026  Vladimir Makarov <vmakarov@gcc.gnu.org>
#
# Usage: test.sh
#

SRCDIR=`dirname $0`

GCC='gcc -O0 -w -g'
outfile=./a.out

for i in 01 02;do
    if $GCC -I$SRCDIR/.. -I$SRCDIR $SRCDIR/test$i.c $SRCDIR/../gecko.c -o $outfile && $outfile 0; then
	echo test$i -- ok
    else
	echo test$i -- FAIL
	exit 1
    fi
done

for i in 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22;do
    if $GCC -I$SRCDIR/.. -I$SRCDIR $SRCDIR/test$i.c $SRCDIR/../gecko.c -o $outfile && $outfile 1 0  2>&1  | cmp - $SRCDIR/test$i.out; then
	echo test$i -- ok
    else
	echo test$i -- FAIL
	exit 1
    fi
done

for i in 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37;do
    if $GCC -I$SRCDIR/.. -I$SRCDIR $SRCDIR/test$i.c $SRCDIR/../gecko.c -o $outfile && $outfile 0 0  2>&1  | cmp - $SRCDIR/test$i.out; then
	echo test$i -- ok
    else
	echo test$i -- FAIL
	exit 1
    fi
done

if type flex >/dev/null 2>&1; then
        i=40
        flex -o ansic.c $SRCDIR/ansic.l
	if $GCC -I$SRCDIR/.. -I$SRCDIR $SRCDIR/test$i.c $SRCDIR/../gecko.c -o $outfile; then
		if $outfile 0 0 <$SRCDIR/test.i; then
			echo Big C parsing: test$i -- ok
		else
			echo Big C parsing: test$i -- FAIL
			exit 1
		fi
	fi
fi

rm -f $outfile ansic.c
exit 0
