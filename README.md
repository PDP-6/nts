PDP-6 non-timesharing system
============================

This repository is a collection of various MIT PDP-6
non-timesharing programs.
For most of the files the source had to be reconstructed
from binaries found on DECtape images or the MIT archive.

What we have:

- MACDMP
- TECO
- MIDAS
- DDT
- STINK
- LISP

cd to dirs and do
`../_tools/mkdectape < tape.cmd` to create the tapes in dta format.
to convert to dtr (used by aap/pdp6), do `dta2dtr < in.dta > out.dtr`.

TODO
====

what exactly is OPDDT?

system tape:
- older TECO version
- various STINK versions
- MARK and SYSGEN
- spacewar

- TD10 system
- ?different memory size system
- 
