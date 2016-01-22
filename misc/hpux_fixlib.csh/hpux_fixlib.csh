#! /bin/csh
# This file is part of Prospero
# Copyright (c) 1995, Univerisity of Southern California
# Author: Steve Augart, 9/26/95
#
# This CSH script takes a single argument: the name of a library archive file.
# It recreates the library archive file with shorter names.  
# This is useful under HP-UX version A.09.01 A when you get compilation errors
# of the form:
# 
#/bin/ld: ../lib/pfs/libpfs.a(_file.): Not a valid object file (invalid system id)
# In that case, the thing to do is, from the root of the prospero distribution,
# type:
#     csh misc/hpux_fixlib.csh
set libname=$1
set libdir=`dirname $libname`
/bin/rm -rf linkdir
mkdir linkdir
@ index=1
foreach file (*.o)
	ln $file linkdir/$index.o
	@ index++
end
cd linkdir
/bin/rm ../`basename $libname`
ar cr ../`basename $libname` *.o
