#!/bin/bash 

# Debugging script for dwb, needs mercurial and gdb 

BUILDDIR="/tmp/DWB_DEBUG_$UID"
LOGFILE="/tmp/dwb_gdb_$UID.log"


if [ ! -d ${BUILDDIR} ]; then 
  mkdir ${BUILDDIR}
fi
  
cd ${BUILDDIR}
echo $PWD
if [ ! -d ${BUILDDIR}/dwb ]; then 
  hg clone https://bitbucket.org/portix/dwb
  cd ${BUILDDIR}/dwb
else 
  cd ${BUILDDIR}/dwb
  hg pull
  hg up
fi
 
cd ${BUILDDIR}/dwb/src
make debug
gdb -batch -ex "set logging on ${LOGFILE}" -ex "run" -ex "bt" -ex "quit" dwb_d 
