# Tcl Proceedure for automatically creating an Event File name.
# The Tcl proceedure takes up to four arguments. They do not all have to
# be used to create the file name:
#
#    codafile <runNumber> <runType> <config> <splitnb>
#
# Where:      <runNumber> =  The current Run Number (Integer)
#             <runType>   =  The current Run Type   (Integer)
#             <config>    =  The current Configuration (Run Type) Name (String)
#             <splitnb>   =  The split number of the file (integer)
#                               (if multiple files are written for a single run
#                                this is the number of current file to be opened.
#                                It starts at nb=0)
#
# Note : In order to load this proceedure so that it can be used by the CODA component
#        you must specify an additional option (-f) when the component is booted.
#        for example:
#                       coda_er -i -s mysession -n ER1 -t ER -f codafile.tcl
#

proc codafile {rn rt config nb} {
  global env

  if {[catch "set env(CODA_DATA)" res]} {
       set filename ./${config}_$rn.evt.$nb
  } else {
       set filename $env(CODA_DATA)/${config}_$rn.evt.$nb
  }
  return $filename
}

