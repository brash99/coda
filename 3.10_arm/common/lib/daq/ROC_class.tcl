if {"[info command ROC]"!="ROC"} {
    global tcl_modules
    
    lappend tcl_modules "ROC_class.tcl {} {} {\$Id: ROC_class.tcl,v 1.38 2002/10/21 14:51:29 abbottd Exp $}"
    
    class ROC {
	inherit CODA
	
	method   download       {config} @roc_download
	method   prestart       {}       @roc_prestart
	method   go             {}       @roc_go
	method   end            {}       @roc_end
	method   pause          {}       @roc_pause
	
	method   exit           {}       {}
	
	method   check_bb       {}       @check_bb

	method   roc_dump       {}       @roc_dump
	method   roc_cleanup    {}       @roc_cleanup
	
	method   part_stats     {p}      @partStats_cmd
	method   part_stats_all {}       @partStatsAll_cmd
	method   part_reinit_all {}      @partReInitAll_cmd
	
	method   token_handler  {name limit id}       @roc_token_handler
	
	method   test_event     {}       @roc_test_event
	method   test_link      {nBuffers}            @roc_test_link

	method   open_links     {}       {}
	method   close_links    {}       {}
	
	method   rol0           {}       {}
	method   rol1           {}       {}
	method   token_debug    {}       @token_debug
	method   roc_constructor           {args} @roc_constructor
	method udp_RPC_handler {s f} {}

	constructor      {sess} {CODA::constructor $sess} {
	    global env
	    roc_constructor $sess
	    set session $sess
	    
	    if { ![catch "set env(TOKEN_PORT)" res] } {
		set udp_port [dp_connect -udp $env(TOKEN_PORT)]
		dp_filehandler [lindex $udp_port 0] r "itcl_context $this ::ROC udp_RPC_handler"
	    }
	    set output_file ""
	    set output_type ""
	    set current_file ""
	}
	
	destructor                      @roc_destructor
	
	public variable bigendian_out 1
	public variable rols    {}
	public variable config  ""
	public variable runtype ""
	public variable inputs  ""
	public variable outputs ""
	public variable next    ""
	public variable output_type
	public variable output_file
	public variable current_file
	private variable links ""
	public variable session
	public variable prime ""
	public variable token_interval @token_interval
	public variable got_next_roc @got_next_roc
	public variable async_roc 0
	public variable first
	public variable randy_factor 32
    }
    
    body ROC::udp_RPC_handler {s f} {
	set message [dp_receiveFrom $f 1024 -noaddr]
	#puts "mess $message"
	catch $message res
	#puts "result $res"
    }

    body ROC::open_links {} {
	
	database query "select name,inputs,outputs,next,first from $config where name='$name'"
	
	set res [database get next]
	set inputs  [lindex $res 1]
	set outputs [lindex $res 2]
	set next    [lindex $res 3]
	set first   [lindex $res 4] 
	if { "$next" != "" } {
	    set got_next_roc 1
	    #DP_ask $next status
	    #dp_socketOption [NS_FindServiceByName $next] nodelay 1
	    puts "next ROC in token chain is $next"
	} else {
	    set got_next_roc 0
	    puts "no next ROC in token chain"
	}
	dalogmsg "INFO" "opening datalink to $outputs"

	if {"$outputs" != ""} {
	    case [lindex $outputs 0] {
		file* {
		    set output_type [lindex $outputs 0]
		    database query "select value from [set config]_option where name='dataFile'"
		    set output_file [database get next]
		    if { "$output_file" == "" } {
			database query "select code from [set config] where name='$output_type'"
			set a [database get next]
			set a [lindex $a 0]
			set output_file [lindex $a 0]
		    } 
		    if { "$output_file" == "" } {
			set output_file "test.dat"
		    }
		    puts "binary format file $output_file"
		}
		dd* {
		    set output_type [lindex $outputs 0]
	        }
		coda* {
		    set output_type [lindex $outputs 0]
		    database query "select value from [set config]_option where name='dataFile'"
		    set output_file [database get next]
		    if { "$output_file" == "" } {
			database query "select code from [set config] where name='$output_type'"
			
			set a [database get next]
			set a [lindex $a 0]
			set output_file [lindex $a 0]
		    }
		    if { "$output_file" == "" } {
			set output_file "test.dat"
		    }
		    puts "CODA format file $output_file"
		}
		debug* {
		    set async_roc 0
		    set output_type [lindex $outputs 0]
		}
		none* {
		    set async_roc 0
		    set output_type [lindex $outputs 0]
		}
		default {
		    foreach link $outputs {
			set res [split $link :]
			if { [llength $res] == 1 } {
			    LINK $link $session $name $link out
			} else {
			    LINK [lindex $res 0] $session $name [lindex $res 0] out
			}
			lappend links [lindex $res 0]
			database query "select first from $config where name='[lindex $res 0]'"
			set res2 [database get next]
			if { "$res2" == "yes" } {
			    set prime [lindex $res 0]
			}
		    }
		    set async_roc 0
		    set output_type network
		    puts "prime is $prime next is $next"
		    
		    return
		}
	    }
	} else {
	    puts "No output type specified - set to none"
	    if { "$output_type" == "network" } {
		set output_type none
	    }
            set output_type none
	}
	set links ""
	set prime ""
	set next ""
    }    
    
    body ROC::close_links {} {
	foreach link $links {
	    catch "delete object $link"
	}
	set links ""
    }
    
    body ROC::exit {} {
	global os_name
	
	set async_roc 1
	end
	close_links
	roc_cleanup
	set state configured
    }
    
    proc every {time args} {
	eval  $args
	dp_after $time every $time $args
    }
    
    proc poll {} {}

    proc ftest {} {
	ROC2 download rocalone
	ROC2 prestart
    }

    proc fgen {runnb runty config} {
	puts "filename for run $runnb type $runty config $config"
	return test.data
    }

    proc test_start {} {
	global fd
	set fd [open "stats.dat" w]
	ev_size
    }

    proc ev_size {} {
	global fd
	set stats [ROC2 statistics]
	set size [ROC2 cget -user_flag3]

	puts $fd "$size [lindex $stats 1] [lindex $stats 3]"
	puts  "$size [lindex $stats 1] [lindex $stats 3]"

	set size [expr $size + 100]

	ROC2 configure -user_flag3 $size

	dp_after 5000 ev_size
    }
}


