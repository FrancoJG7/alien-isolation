#!/bin/bash
s=$1; shift
/tmp/claude-0/gh/ghidra_11.4.2_PUBLIC/support/analyzeHeadless /tmp/claude-0/gh/pgr GR -process grand.dll -noanalysis -readOnly -scriptPath /tmp/claude-0/gh/scripts -postScript $s.java "$@" > /tmp/claude-0/gh/lastg.log 2>&1
grep -E "ERROR|Exception" /tmp/claude-0/gh/lastg.log | grep -v RTTI | head -5
