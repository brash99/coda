# Class definition for CODA class.
#
# The CODA class is common to all CODA components and
# contains all of the basic functionality of the CODA
# state transition diagram.
#
# CODA inherits RPC so that each component is an RPC
# server...

class CODA {
    public  variable local_input ""
    private variable prime_input ""
    private variable poll_rate   ""
    private variable targets     ""
    private variable class "0"
    public  variable state "unknown"
    public  variable name  "unknown"
    public  variable debug
    public  variable nlongs 
    public  variable nevents 

    public  variable event_rate 0
    public  variable data_rate 0

    public  variable codaid  
    public  variable targetOK 
    public  variable session

    public variable user_flag1 ""
    public variable user_flag2 ""
    public variable user_flag3 0
    public variable user_flag4 0
    public variable bigendian 1
    public variable telnetServer

    private variable oldtime 0.0
    private variable oldevents 0
    private variable oldlongs 0

    method   status         {{arg ""}}   {}
    method   download       {}       {}
    method   prestart       {}       {}
    method   go             {}       {}
    method   end            {}       {}
    method   pause          {}       {}

    method   exit           {}       {}
    method   zap            {}       {}
    method   dalogmsg       {sev msg} @logmsgcmd

    private method  AcceptTelnetConnection {status file} {}
    private method  ShutdownTelnet {file} {}
    private method  ProcessData {status file} {}
    method connectStdio {file} @CODAConnectStdio
    method disconnectStdio {} @CODADisconnectStdio

    method   version        {}       {
	global tcl_modules coda_version
	puts "CODA version $coda_version"
	puts "=========================="
	foreach module $tcl_modules  {
	    puts "module : [lindex $module 0]"
	    if {"[lindex $module 2]" != ""} {
		puts "         Compiled by [lindex $module 2] on [lindex $module 1]"
	    }
	    puts "         CVS info :"
	    set cvs [lindex $module 3]
	    puts "                   File version: [lindex $cvs 2]"
	    puts "                   Commit by   : [lindex $cvs 5]"
	    puts "                   Date        : [lindex $cvs 3]"
	    puts "                   Time        : [lindex $cvs 4]"
	    puts "                   File status : [lindex $cvs 6]"
	}
    }

    private method setRates {} {
	set newtime [ns_time]
	set timediff [expr $newtime - $oldtime]
	set event_rate [expr ($nevents - $oldevents)/$timediff]
	set data_rate [expr 4*($nlongs - $oldlongs)/$timediff]
	set oldlongs $nlongs
	set oldevents $nevents
	set oldtime $newtime
    }

    method   statistics {} {
	return [list $nevents $event_rate $nlongs $data_rate]
    }

    constructor             {sess} { 
	global os_name
	set session $sess
	if { "$os_name" == "vxworks" } {
	    set rv [dp_connect -server 7030 -reuseAddr]
	    
	    set telnetServer [lindex $rv 0]
	    set port [lindex $rv 1]
	    dp_filehandler $telnetServer re "itcl_context $this ::CODA AcceptTelnetConnection"
	    dp_atexit appendUnique "close $telnetServer"
	    dp_atclose $telnetServer append "dp_shutdown $telnetServer both;dp_filehandler $telnetServer;catch {dp_atexit delete \"close $telnetServer\"};set telnetServer \"\""
	}
    } @coda_constructor

    destructor              @coda_destructor
}

body CODA::ProcessData {status file} {
    
    if { "$status" == "e" } {
	puts "close $file"
	disconnectStdio	
	close $file
    }

}

body CODA::AcceptTelnetConnection {status file} {
    if {[string compare $status e] == 0} {
	close $file;
	return; 
    }
    set connection [dp_accept $file]
    set newFile [lindex $connection 0]
    set inetAddr [lindex $connection 1]
    puts "new connection $newFile"
    dp_atclose $newFile prepend "itcl_context $this ::CODA ShutdownTelnet $newFile"
    dp_atclose $file appendUnique "itcl_context $this ::CODA ShutdownTelnet $newFile"
    dp_atexit appendUnique "close $newFile"
    dp_filehandler $newFile e "itcl_context $this ::CODA ProcessData"

    connectStdio $newFile
}

#
# Shut down a connection by telling the other end to shutdown and
# removing the filehandler on this file.
#

body CODA::ShutdownTelnet {file} {
    disconnectStdio

    catch "dp_filehandler $file"
    dp_atexit delete "close $file"
    catch "dp_atclose $file clear"
    dp_atclose $server delete "itcl_context $this ::CODA ShutdownTelnet $file"
}

body CODA::status {{arg ""}} { 
    if { "$arg" != "" } {
	$this configure -state $arg
	database query "UPDATE process SET state='$arg' WHERE name='$name'"
    }
    return $state
}

body CODA::download {} {
    status downloaded
    return $state
}

body CODA::prestart {} {
    status paused
    return $state
}

body CODA::go {} {
    status active
    return $state
}

body CODA::end {} {
    status downloaded
    return $state
}

body CODA::pause {} {
    status paused
    return $state
}

body CODA::exit {} {
    global os_name

    end
    set state configured
}

body CODA::zap {} {
    global os_name

    if {"$os_name" != "vxworks"} {
	::exit
    }
}

