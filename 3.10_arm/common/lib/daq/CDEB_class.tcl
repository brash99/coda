# ITCL class definition for the dummy event builder class

if {"[info command CDEB]"!="CDEB"} {
global tcl_modules

lappend tcl_modules "CDEB_class.tcl {} {} {\$Id: CDEB_class.tcl,v 2.34 1998/11/17 19:14:06 abbottd Exp $}"

class CDEB {
    inherit CODA

    method   download       {conf}   {} 
    private  method   c_download       {conf}   @deb_download
    method   shutdown_build {} @shutdown_build
    method   prestart       {}       {}
    method   c_prestart     {}       @deb_prestart
    method   go             {}       @deb_go
    method   end            {}       @deb_end 
    method   pause          {}       {}

    method   dump           {}       {}

    method   check_network  {}       {}

    method   part_stats     {p}      @partStats_cmd
    method   part_stats_all {}       @partStatsAll_cmd
    method   part_create    {name size count incr} @partCreate_cmd
    method   part_free_all  {}       @free_all_cmd
    method   part_reinit_all {}      @partReInitAll_cmd
    method   test_event     {}       {}
    method   open_links     {}       {}
    method   close_links    {}       {}
    method   token_handler  {name limit id}     @token_handler 
    method   init_in_thread {name}                @CDEB_init_thread
    method   masks          {} @masks_cmd
    method   force_end      {} @force_end
    method   exit           {}       {}
    method   set_roc_mask   {mask}
    method   ev_stats       {} {}
    method   link_debug     {} @LINK_debug
    method   frag_sized     {} @CDEB_fragment_sizes


    private method c_constructor {sess} @deb_constructor
    constructor      {sess} {CODA::constructor $sess} {
	c_constructor $sess
	
	set output_file ""
	set current_file ""
	set output_type ""
	set first_eb ""
	set current_link ""
    }
    destructor @deb_destructor

    public variable private 0

# public variables follow here...

    public variable config  ""
    public variable inputs  ""
    public variable outputs "" 
    public variable next    ""
    public variable first_eb ""
    public variable target  ""
    public variable links   ""

    public variable current_link
    public variable current_limit 
    public variable new_limit     
    public variable current_id    
    public variable last_id       
    public variable token_interval 
    public variable first         
    public variable last          
    public variable nrocs         
    public variable last_buffer   
    public variable last_link ""
    public variable token_count
    public variable output_type
    public variable output_switch
    public variable output_file
    public variable current_file
    public variable send_tokens
    public variable nlinks
    # These variables manage the buffer pool

    private variable session
    public  variable eventsPerToken
    public  variable debugLevel
    public  variable multi_mode
}

struct_typedef eb_priv {struct
    {int token_count}
    {int current_limit}
    {int new_limit}
    {int current_id}
    {int last_id}
    {int token_interval}
    {int first}
    {int last}
    {int nrocs}
    {int starting}
    {int last_bufnb}
    {int codaid}
    {long frag_hdr_pool}
    {long*32 roc_stream}
    {int*32 roc_id}
    {int*32 roc_nb}
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

body CDEB::ev_stats {} {
	struct_new stats int*32 int*32@roc_queue_evsize
	

}
body CDEB::prestart {} {
    dalogmsg INFO "prestart"
    if { [catch "close_links"] } {
	dalogmsg "ERROR" "prestart aborted during close_links"
	return 
    }

    dp_after 2

    if { [catch "open_links"] } {
	dalogmsg "ERROR" "prestart aborted during open_links"
	return
    }
    c_prestart
}

body CDEB::download {conf} {
    set config $conf
    dalogmsg INFO "download"
    c_download $conf
    dalogmsg INFO "downloaded"
    $this status downloaded
}

body CDEB::pause {} {
    dalogmsg INFO "paused "
    $this status paused
}

body CDEB::exit {} {
    global os_name

    set state downloaded
    end
    shutdown_build

    set thelinks [info objects -class LINK]
    foreach l $thelinks {
	delete object $l
    }

    set state configured
}

body CDEB::open_links {} {

    struct_new EBp eb_priv eb_priv@$private
    
    for {set ix 0} {$ix < 32} {incr ix} {
	set EBp(roc_id.$ix) -1
    }
    database query "select value from ${config}_option where name='tokenInterval'"
    set token_interval [database get next]
    
    database query "select first,next from $config where name='$name'"
    set res [database get next]
    if {  "[lindex $res 0]" == "yes" } {
	set first 1
    } else {
	set first 0
    }

    set next  [lindex $res 1]
    set nlinks 0

    database query "select name,inputs,outputs,next from $config where name='$name'"

    set res [database get next]
    set inputs  [lindex $res 1]
    set outputs [lindex $res 2]
    set next    [lindex $res 3]

    if { "$next" == "" } {
	set last 1
	if { $first == 1 } {
	    set next $name
	}
    }

    set links ""

    set nrocs 0
    set hosts ""
    foreach link $inputs {
	set res [split $link :]
	if { [llength $res] == 1 } {
	    LINK $link $session $link $name in 
	} else {
	    database query "select outputs from $config where name ='[lindex $res 0]'"
	    set res2 [lindex [database get next] 0]
	    set ix [lsearch -regexp $res2 "$name:.*"]
	    set link_in [lindex $res2 $ix]
	    set in_host [split $link_in :]
	    LINK [lindex $res 0] $session [lindex $res 0] $name in "" [lindex $in_host 1]
	    if { "[lsearch -exact $hosts [lindex $in_host 1]]" == "-1" } {
		lappend hosts [lindex $in_host 1]
	    }
	}
	lappend links [lindex $res 0]

	#init_in_thread [lindex $res 0]
    }

    set nlinks [llength $links]

    if { $nlinks >4 } {
	set nlinks 4
    }

    puts "$name has $nlinks input links"

    foreach link $links { 
	database query "SELECT first,next FROM $config WHERE name='$link'"
	
	set res [database get next]
	set roc_first [lindex $res 0]
	set roc_next  [lindex $res 1]
	
	database query "SELECT id FROM process WHERE name='$link'"
	
	set res [database get next]
	set roc_id    [lindex $res 0]
	
	if { "$roc_next" == "" } { 
	    set last_link $link
	    set last_id   $roc_id
	    puts "last ROC is $last_id"
	}

	if { "$roc_first" == "yes" } {
	    set current_link $link
	    set current_id   $roc_id
	    if { [catch "DP_ask $link status"] } {
		dalogmsg "ERROR" "Data link $link cannot be started, target system down"
		error "Data link $link cannot be started, target system down"
	    }
	    #dp_socketOption [NS_FindServiceByName $link] nodelay 1
	}

	set EBp(roc_id.$nrocs) $roc_id
	set EBp(roc_nb.$roc_id) $nrocs
	incr nrocs
    }

    set output_type "DD System"
    set output_file ""
    set output_switch 0

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
		set output_type none
		set output_switch 3
	    }
	    set output_switch 1
	}
	debug* {
	    set output_type [lindex $outputs 0]
	    set output_switch 2
	}
	none* {
	    set output_type [lindex $outputs 0]
	    set output_switch 3
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
		set output_type none
		set output_switch 3
		set output_file test.dat
	    }
	    dalogmsg "INFO" "Output is to data file $output_file"
	    set output_switch 4
	}
	default {
	    set output_switch 0
	}
    }    
    puts "open links"
}

body CDEB::close_links {} {
    foreach link $links {
	catch "delete object $link"
    }
    set links ""
    set current_link ""
}

body CDEB::set_roc_mask {mask} {
    catch "database query \"SELECT name,value FROM ${config}_option WHERE name='rocMask'\" " res
    if [database get rows] {
	set res [database query "UPDATE ${config}_option SET value='$mask' WHERE name='rocMask'"]
    } else {
	set res [database query "INSERT INTO ${config}_option (name,value) VALUES ('rocMask','$mask')"]
    }
}


proc test_20by1 {} {
    EB1 download twentyone
    DP_ask ROC1 download twentyone
    DP_ask ROC2 download twentyone
    DP_ask ROC3 download twentyone
    DP_ask ROC4 download twentyone
    DP_ask ROC5 download twentyone
    DP_ask ROC6 download twentyone
    DP_ask ROC7 download twentyone
    DP_ask ROC8 download twentyone
    DP_ask ROC9 download twentyone
    DP_ask ROC10 download twentyone
    DP_ask ROC11 download twentyone
    DP_ask ROC12 download twentyone
    DP_ask ROC13 download twentyone
    DP_ask ROC14 download twentyone
    DP_ask ROC15 download twentyone
    DP_ask ROC16 download twentyone
    DP_ask ROC17 download twentyone
    DP_ask ROC18 download twentyone
    DP_ask ROC19 download twentyone
    DP_ask ROC20 download twentyone
    EB1 prestart
    DP_ask ROC1 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    DP_ask ROC5 prestart
    DP_ask ROC6 prestart
    DP_ask ROC7 prestart
    DP_ask ROC8 prestart
    DP_ask ROC9 prestart
    DP_ask ROC10 prestart
    DP_ask ROC11 prestart
    DP_ask ROC12 prestart
    DP_ask ROC13 prestart
    DP_ask ROC14 prestart
    DP_ask ROC15 prestart
    DP_ask ROC16 prestart
    DP_ask ROC17 prestart
    DP_ask ROC18 prestart
    DP_ask ROC19 prestart
    DP_ask ROC20 prestart
    DP_ask ROC1 go
    DP_ask ROC2 go
    DP_ask ROC3 go
    DP_ask ROC4 go
    DP_ask ROC5 go
    DP_ask ROC6 go
    DP_ask ROC7 go
    DP_ask ROC8 go
    DP_ask ROC9 go
    DP_ask ROC10 go
    DP_ask ROC11 go
    DP_ask ROC12 go
    DP_ask ROC13 go
    DP_ask ROC14 go
    DP_ask ROC15 go
    DP_ask ROC16 go
    DP_ask ROC17 go
    DP_ask ROC18 go
    DP_ask ROC19 go
    DP_ask ROC20 go
}

proc test {} {
    EB2 download TEST_CONFIG3
    DP_ask ROC2 download TEST_CONFIG3
    EB2 prestart
    DP_ask ROC2 prestart
    # EB2 go
    # DP_ask ROC2 go
}

proc test_end {} {
    DP_ask ROC2 end
    EB2 end
}

proc loop_test {} {
    test
    dp_after 10000 test_end
    dp_after 15000 loop_test
}
proc er_test {} {
    DP_ask ER2 download TEST_CONFIG4
    EB2 download TEST_CONFIG4
    DP_ask ROC2 download TEST_CONFIG4

    DP_ask ER2 prestart
    EB2 prestart
    DP_ask ROC2 prestart

    DP_ask ER2 go
    EB2 go
    DP_ask ROC2 go
}

proc er_test_end {} {
    DP_ask ROC2 end
    EB2 end
    DP_ask ER2 end
}

proc test2 {} {
    EB2 download TEST_2x1
    DP_ask ROC2 download TEST_2x1
    DP_ask ROC3 download TEST_2x1
    EB2 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    EB2 go
    DP_ask ROC2 go
    DP_ask ROC3 go
}

proc test2b {} {
    dp_after 1000 EB2 download two_portEB
    dp_after 2000 DP_ask ROC2 download two_portEB
    dp_after 4000 DP_ask ROC3 download two_portEB
    dp_after 8000 EB2 prestart
    dp_after 10000 DP_ask ROC2 prestart
    dp_after 12000 DP_ask ROC3 prestart
    dp_after 16000 EB2 go
    dp_after 18000 DP_ask ROC2 go
    dp_after 20000 DP_ask ROC3 go
}

proc test3 {} {
    EB2 download test3x1
    DP_ask ROC2 download test3x1
    DP_ask ROC3 download test3x1
    DP_ask ROC4 download test3x1
    EB2 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    EB2 go
    dp_after 10000 DP_ask ROC2 go
    dp_after 11000 DP_ask ROC3 go
    dp_after 12000 DP_ask ROC4 go
}

proc test4 {} {
    EB2 download TEST_4x1
    DP_ask ROC2 download TEST_4x1
    DP_ask ROC3 download TEST_4x1
    DP_ask ROC4 download TEST_4x1
    DP_ask ROC5 download TEST_4x1
    EB2 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    DP_ask ROC5 prestart
    EB2 go
    DP_ask ROC2 go
    DP_ask ROC3 go
    DP_ask ROC4 go
    DP_ask ROC5 go
}

proc test5 {} {
    EB2 download TEST_5x1
    DP_ask ROC2 download TEST_5x1
    DP_ask ROC3 download TEST_5x1
    DP_ask ROC4 download TEST_5x1
    DP_ask ROC5 download TEST_5x1
    DP_ask ROC1 download TEST_5x1
    EB2 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    DP_ask ROC5 prestart
    DP_ask ROC1 prestart
    EB2 go
    DP_ask ROC2 go
    DP_ask ROC3 go
    DP_ask ROC4 go
    DP_ask ROC5 go
    DP_ask ROC1 go
}
proc test5b {} {
    EB1 download another5
    DP_ask ROC6 download another5
    DP_ask ROC7 download another5
    DP_ask ROC8 download another5
    DP_ask ROC9 download another5
    DP_ask ROC10 download another5
    EB1 prestart
    DP_ask ROC6 prestart
    DP_ask ROC7 prestart
    DP_ask ROC8 prestart
    DP_ask ROC9 prestart
    DP_ask ROC10 prestart
    EB1 go
    DP_ask ROC6 go
    DP_ask ROC7 go
    DP_ask ROC8 go
    DP_ask ROC9 go
    DP_ask ROC10 go
}

proc test5_end {} {
    DP_ask ROC2 end
    DP_ask ROC3 end
    DP_ask ROC4 end
    DP_ask ROC5 end
    DP_ask ROC1 end
}

proc test10 {} {
    EB2 download tenone
    DP_ask ROC1 download tenone
    DP_ask ROC2 download tenone
    DP_ask ROC3 download tenone
    DP_ask ROC4 download tenone
    DP_ask ROC5 download tenone
    DP_ask ROC6 download tenone
    DP_ask ROC7 download tenone
    DP_ask ROC8 download tenone
    DP_ask ROC9 download tenone
    DP_ask ROC10 download tenone
    EB2 prestart
    dp_after 1000 DP_ask ROC1 prestart
    dp_after 2000 DP_ask ROC2 prestart
    dp_after 3000 DP_ask ROC3 prestart
    dp_after 4000 DP_ask ROC4 prestart
    dp_after 5000 DP_ask ROC5 prestart
    dp_after 6000 DP_ask ROC6 prestart
    dp_after 7000 DP_ask ROC7 prestart
    dp_after 8000 DP_ask ROC8 prestart
    dp_after 9000 DP_ask ROC9 prestart
    dp_after 10000 DP_ask ROC10 prestart
    dp_after 12000 EB2 go
    dp_after 13000 DP_ask ROC1 go
    dp_after 14000 DP_ask ROC2 go
    dp_after 15000 DP_ask ROC3 go
    dp_after 16000 DP_ask ROC4 go
    dp_after 17000 DP_ask ROC5 go
    dp_after 18000 DP_ask ROC6 go
    dp_after 19000 DP_ask ROC7 go
    dp_after 20000 DP_ask ROC8 go
    dp_after 21000 DP_ask ROC9 go
    dp_after 22000 DP_ask ROC10 go
}

proc test10_end {} {
    EB2 end
    DP_ask ROC1 end
    DP_ask ROC2 end
    DP_ask ROC3 end
    DP_ask ROC4 end
    DP_ask ROC5 end
    DP_ask ROC6 end
    DP_ask ROC7 end
    DP_ask ROC8 end
    DP_ask ROC9 end
    DP_ask ROC10 end
}
proc test15 {} {
    EB2 download test15x1
    DP_ask ROC1 download test15x1
    DP_ask ROC2 download test15x1
    DP_ask ROC3 download test15x1
    DP_ask ROC4 download test15x1
    DP_ask ROC5 download test15x1
    DP_ask ROC6 download test15x1
    DP_ask ROC7 download test15x1
    DP_ask ROC8 download test15x1
    DP_ask ROC9 download test15x1
    DP_ask ROC10 download test15x1  
    DP_ask ROC11 download test15x1  
    DP_ask ROC12 download test15x1  
    DP_ask ROC13 download test15x1  
    DP_ask ROC14 download test15x1  
    DP_ask ROC15 download test15x1  
    EB2 prestart
    DP_ask ROC1 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    DP_ask ROC5 prestart
    DP_ask ROC6 prestart
    DP_ask ROC7 prestart
    DP_ask ROC8 prestart
    DP_ask ROC9 prestart
    DP_ask ROC10 prestart
    DP_ask ROC11 prestart
    DP_ask ROC12 prestart
    DP_ask ROC13 prestart
    DP_ask ROC14 prestart
    DP_ask ROC15 prestart
    EB2 go
    DP_ask ROC1 go
    DP_ask ROC2 go
    DP_ask ROC3 go
    DP_ask ROC4 go
    DP_ask ROC5 go
    DP_ask ROC6 go
    DP_ask ROC7 go
    DP_ask ROC8 go
    DP_ask ROC9 go
    DP_ask ROC10 go
    DP_ask ROC11 go
    DP_ask ROC12 go
    DP_ask ROC13 go
    DP_ask ROC14 go
    DP_ask ROC15 go
}

proc test20 {} {
    EB2 download twentyone
    DP_ask ROC1 download twentyone
    DP_ask ROC2 download twentyone
    DP_ask ROC3 download twentyone
    DP_ask ROC4 download twentyone
    DP_ask ROC5 download twentyone
    DP_ask ROC6 download twentyone
    DP_ask ROC7 download twentyone
    DP_ask ROC8 download twentyone
    DP_ask ROC9 download twentyone
    DP_ask ROC10 download twentyone  
    DP_ask ROC11 download twentyone  
    DP_ask ROC12 download twentyone  
    DP_ask ROC13 download twentyone  
    DP_ask ROC14 download twentyone  
    DP_ask ROC15 download twentyone  
    DP_ask ROC16 download twentyone  
    DP_ask ROC17 download twentyone  
    DP_ask ROC18 download twentyone  
    DP_ask ROC19 download twentyone  
    DP_ask ROC20 download twentyone  
    EB2 prestart
    DP_ask ROC1 prestart
    DP_ask ROC2 prestart
    DP_ask ROC3 prestart
    DP_ask ROC4 prestart
    DP_ask ROC5 prestart
    DP_ask ROC6 prestart
    DP_ask ROC7 prestart
    DP_ask ROC8 prestart
    DP_ask ROC9 prestart
    DP_ask ROC10 prestart
    DP_ask ROC11 prestart
    DP_ask ROC12 prestart
    DP_ask ROC13 prestart
    DP_ask ROC14 prestart
    DP_ask ROC15 prestart
    DP_ask ROC16 prestart
    DP_ask ROC17 prestart
    DP_ask ROC18 prestart
    DP_ask ROC19 prestart
    DP_ask ROC20 prestart
    EB2 go
    DP_ask ROC1 go
    DP_ask ROC2 go
    DP_ask ROC3 go
    DP_ask ROC4 go
    DP_ask ROC5 go
    DP_ask ROC6 go
    DP_ask ROC7 go
    DP_ask ROC8 go
    DP_ask ROC9 go
    DP_ask ROC10 go
    DP_ask ROC11 go
    DP_ask ROC12 go
    DP_ask ROC13 go
    DP_ask ROC14 go
    DP_ask ROC15 go
    DP_ask ROC16 go
    DP_ask ROC17 go
    DP_ask ROC18 go
    DP_ask ROC19 go
    DP_ask ROC20 go
}

}

