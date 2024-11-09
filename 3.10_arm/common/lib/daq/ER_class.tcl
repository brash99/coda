# ITCL class definition for the dummy event builder class
if {"[info command ER]"!="ER"} {
global tcl_modules

lappend tcl_modules "ER_class.tcl {} {} {\$Id: ER_class.tcl,v 2.18 2000/01/14 21:50:40 abbottd Exp $}"
class ER {
    inherit CODA
    
    method   download       {conf}   {} 
    method   download_c     {conf}   @ER_download_c 
    method   prestart       {}       {}
    method   prestart_c     {}       @ER_prestart
    method   go             {}       {}
    method   end            {}       {}
    method   pause          {}       {}
    
    method   daq            {arg}    @ER_daq_cmd
    method   conf           {}       {}
    method   flush           {}      @ER_flush_dd

    constructor      {sess} {CODA::constructor $sess} @log_constructor
    
    destructor                      {}
    
    public  variable config  ""
    private variable session

    public  variable flushinterval "1000"
    public  variable SPLITMB "2047"
    public  variable RECL    "32678"
    public  variable bgid ""
    public  variable EvDumpLevel "0"

    public variable outputs ""
    public variable output_type
    public variable output_switch
    public variable output_file "test.dat"
    public variable filename ""
    public variable current_file ""
    public variable file_events 0
    public variable file_nlongs 0
    public variable file_nerrors 0

    public variable nEnd 1
}

struct_typedef fev {struct
    {int shmid}
    {int dboff}
    {int p2da}
    {int len}
    {int ctlw1}
    {int ctlb1}
    {int ctlw2}
    {int ctlb2}
}

struct_typedef ev_buf {struct
    {ulong len}
    {ulong*0 data}
}

body ER::download {conf} {
    puts "ER::download $conf called"
    set config $conf
    dalogmsg INFO "download"
    set RECL 32768
    conf
    download_c $conf

    dalogmsg INFO "downloaded"
    $this status downloaded
}

body ER::prestart {} {
    puts "ER::prestart called"
    dalogmsg INFO "prestarting"
    flush
    set nEnd 1
    prestart_c

    puts "output_switch = $output_switch"
    case $output_switch {
     2  {
	daq debug
	dalogmsg WARN "Output to debug"
     }
     3  {
	daq none
	dalogmsg WARN "Output to none"
     }
     default  {
        daq open
        dalogmsg INFO "Output to file '$current_file' for writing"
        }
    }
    $this status paused
}

body ER::go {} {
    puts "ER::go called"
    dalogmsg INFO "activating"
    daq resume
    dalogmsg INFO "activated"
    $this status active
}

body ER::pause {} {
    puts "ER::pause called"
    daq pause
    $this status paused
}

body ER::end {} {
    puts "ER::end called"
    dalogmsg INFO "ending"
    daq close
    $this status downloaded
    dalogmsg INFO "ended"
}

body ER::conf {} {

    database query "select name,value from ${config}_option where name='flushinterval'"
    set res [database get next]
    if { "$res" != ""} {
	set flushinterval [lindex $res 1]
    }
    database query "select name,value from ${config}_option where name='SPLITMB'"
    set res [database get next]
    if { "$res" != ""} {
	puts "SPLITMB was $SPLITMB"
	set SPLITMB [expr [lindex $res 1] << 20 ]
	puts "SPLITMB is $SPLITMB"
    } else {
	set SPLITMB 0
    }
    database query "select name,value from ${config}_option where name='RECL'"
    set res [database get next]

    if { "$res" != ""} {
	set RECL [lindex $res 1]
	puts "RECL was $RECL"
    } else {
	set RECL 32768
    }

    database query "select name,value from ${config}_option where name='EvDumpLevel'"
    set res [database get next]
    if { "$res" != ""} {
	set EvDumpLevel [lindex $res 1]
    }
    database query "select name,inputs,outputs from $config where name='$name'"
    set res [database get next]

    set inputs ""
    set output_type "coda"

    if { "$res" != ""} {
	set inputs  [lindex $res 1]
	set outputs [lindex $res 2]
	set output_type [lindex $outputs 0]
    } 

    set output_switch 3

    puts -nonewline "$name will output to "

    case $output_type {
	file* {
	    
	    database query "select value from [set config]_option where name='dataFile'"
	    
	    set output_file [database get next]
	    if { "$output_file" == "" } {
		database query "select code from [set config] where name='$output_type'
		set a [database get next]
		set a [lindex $a 0]
		set output_file [lindex $a 0]

                if { "[lindex $a 0]" == "CODA" } {
   	          puts "coda format file $output_file"
	          set output_switch 4
                }
	        if { "[lindex $a 0]" == "BINA" } {
   	          puts "binary format file $output_file"
	          set output_switch 1
                }
	    } else {
	      set output_switch 1
	      puts "binary file  $output_file"
            }
	    if { "$output_file" == "" } {
		set output_file "test.dat"
	    }
	}
	debug* {
	    puts "debug dump"
	    set output_switch 2
	}
	none* {
	    puts "/dev/null"
	    set output_switch 3
	}
	coda* {
	    database query "select value from [set config]_option where name='dataFile'"
	    set output_file [database get next]
	    if { "$output_file" == "" } {
		database query "select code from [set config] where name='$output_type'

		set a [database get next]
		set a [lindex $a 0]
		set output_file [lindex $a 0]
	        if { "[lindex $a 0]" == "CODA" } {
   	          puts "coda format file $output_file"
	          set output_switch 4
                }
	        if { "[lindex $a 0]" == "BINA" } {
   	          puts "binary format file $output_file"
	          set output_switch 1
                }
	    } else {
  	      puts "coda format file $output_file"
	      set output_switch 4
            }
	    if { "$output_file" == "" } {
		set output_file "test.dat"
	    }
	}
	default {
	    puts "invalid output type $output_type"
	}
    }
    if {"$output_file" == "" } {
       set output_file "test.dat"
    } 
    puts ""
}    
}
