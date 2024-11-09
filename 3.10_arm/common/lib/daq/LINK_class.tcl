# class to handle data links into and out of
# CODA components..

if {"[info command LINK]"!="LINK"} {
global tcl_modules

lappend tcl_modules "LINK_class.tcl {} {} {\$Id: LINK_class.tcl,v 2.25 1998/07/27 19:17:45 heyes Exp $}"

class LINK {
    constructor {session from to dir {callback ""} {host ""}} {}
    destructor  {}

    method constructor_C {session from to dir {callback ""} {host ""}} @LINK_constructor_C
    method destructor_C  {} @LINK_destructor_C

    method cons_connect    {} {} 
#@cons_connect
    method cons_disconnect {} {} 
#@cons_disconnect
    method read            {} {} 
#@cons_read
    method prod_connect    {} {} 
#@prod_connect

    method prod_disconnect {} {} 
#@prod_disconnect

    method statistics {} {}
    method fifo_stats {} @LINK_fifo_status
    method get_outputH {} @LINK_get_outputH
    
    method  link_queue_name {} @LINK_queue_name
	
    public  variable state "down"
    public  variable name  ""
    public  variable ProcessDataC_call  ""
    public  variable server ""
    public  variable newFile ""
    public  variable host ""
    public  variable port ""
    public  variable link {}
    public  variable part
    public  variable link_from
    public  variable link_to
    method   dalogmsg       {sev msg} @logmsgcmd

    private variable direction ""
    private method  AcceptLINKConnection {status file} {}
    private method  ShutdownLINK {file} {}
    private method  ProcessData {status file} @LINK_process_data
    private method  startThread {file} @LINK_thread_init
    method write {data} {}
}

body LINK::destructor {} {
    if { "$link" != ""} {
	ShutdownLINK $link
    }
    catch "dp_atclose clear $newFile"
    catch "dp_shutdown $newFile both"
    catch "dp_filehandler $newFile"
    catch {dp_atexit delete "close $newFile"}
    catch {close $newFile}

    catch "dp_atclose clear $server"
    catch "dp_shutdown $server both"
    catch "dp_filehandler $server"
    catch {dp_atexit delete "close $server"}
    catch {close $server}

    if { "$direction" == "in" } {
	database query "DELETE FROM links WHERE name='$name'"
    }

    destructor_C

}

body LINK::constructor {session from to dir {callback ""} {host ""}} {
    set type TCP
    set direction $dir
    if { "$dir" == "in"} {
	set to [info namespace tail $to]
	set log_name $to
	if { "$from" == "dd" } {set type DD}
    } else {
	set from [info namespace tail $from]
	set log_name $from
	if { "$to" == "dd" } {set type DD}
    }

    set link_from $from
    set link_to $to
    constructor_C $session $from $to $dir $callback $host

    if { "$type" == "DD" } {
	dalogmsg "INFO" "data link $from to $to (DD data link)"
    }
    
    # check to see that the db table exists
    
    set port 0

    if { "$host" == "" } {
	set host [dp_hostname]
    }

    if { "$dir" == "in" } {
	if {"$type" != "DD"} {

	    # TCP data link requires an RPC server

	    if { "$server" == "" } {
		if { "$host" != ""} {
		    set rv [dp_connect -server 0 -host $host -reuseAddr]
		} else {
		    set rv [dp_connect -server 0 -reuseAddr]
		}
		set server [lindex $rv 0]
		set port [lindex $rv 1]
		dp_filehandler $server re "itcl_context $this ::LINK AcceptLINKConnection"
		dp_atexit appendUnique "close $server"
		dp_atclose $server append "dp_shutdown $server both;dp_filehandler $server;catch {dp_atexit delete \"close $server\"};set server \"\""
# bugbug		dp_socketOption $server recvBuffer 63000
		dp_socketOption $server recvBuffer 252000
		dp_socketOption $server autoClose yes
		dp_socketOption $server nodelay 1
		dp_socketOption $server keepAlive yes
	    }
	} else {
	}
	
	global env
	set name "$from->$to"
	catch "database set database $env(EXPID)" res

	catch "database query \"create table links (name char(100) not null,type char(4) not null,host char(30),state char(10),port int)\"" res
	catch "database query \"SELECT name FROM links WHERE name='$name'\" " res
	if [database get rows] {
	    set res [database query "UPDATE links SET host='$host',type='$type',port=$port,state='up' WHERE name='$name'"]
	} else {
	    set res [database query "INSERT INTO links (name,type,host,port,state) VALUES ('$name','$type','$host',$port,'up')"]
	}
    } else {
	if {"$type" != "DD"} {
	    global env
	    set name "$from->$to"
	    catch "database set database $env(EXPID)" res
	    catch "database query \"create table links (name char(100) not null,type char(4) not null,host char(30),state char(10),port int)\"" res
	    catch "database query \"SELECT port,host FROM links WHERE name='$name'\" " res

	    if [database get rows] {
		set res [database get next]
		set host [lindex $res 1]
		set port [lindex $res 0]
		set server [lindex [dp_connect $host $port] 0]
		puts "connect returns $server"

		set return [gets $server]
		if {[lindex $return 1] == "refused:"} {
		    close $server
		    error $return;
		}
#		dp_socketOption $server sendBuffer 40000
#		dp_socketOption $server sendBuffer 52428
#		dp_socketOption $server sendBuffer 50000
#		dp_socketOption $server sendBuffer 5000
		dp_socketOption $server sendBuffer 48000
		dp_socketOption $server noblock no
		dp_socketOption $server autoClose yes
		dp_socketOption $server keepAlive yes
		# Explicitly set nodelay off for data link (VxWorks slows down)
#bugbug		dp_socketOption $server nodelay 1
		dp_atexit appendUnique "close $server"
		dp_atclose $server append "puts \"server $this down close $server\";dp_filehandler $server;dp_atexit delete \"close $server\""
		dalogmsg "INFO" "connected to $to ($host, $port, $server)"
	    } else {
		error "server for link\" $name\" not running."
	    }
	    set link $server
	} else {
	}
    }
    set state "up"
    database query "UPDATE links SET state='up' WHERE name='$name'"
}

body LINK::write {data} {
    if [catch "struct_write -unbuffered $server $data" res] {
	ShutdownLINK $server
	error "link $name shut down"
    }
}

body LINK::statistics {} {
    puts "Statistics for $this"
    puts "FIFO"
    puts "name      waiting  putting  deleting total full"
    puts "----      -------  -------  -------- ------ ----"
    puts "[fifo_stats]"
}

body LINK::AcceptLINKConnection {status file} {
    if {[string compare $status e] == 0} {
	close $file;
	return; 
    }
    set connection [dp_accept $file]
    set newFile [lindex $connection 0]
    set inetAddr [lindex $connection 1]
    dalogmsg "INFO" "link $name connected as $newFile to $inetAddr"
    dp_atclose $newFile prepend "itcl_context $this ::LINK ShutdownLINK $newFile"
    dp_atclose $file appendUnique "itcl_context $this ::LINK ShutdownLINK $newFile"
    dp_atexit appendUnique "close $newFile"
    dp_filehandler $newFile e "itcl_context $this ::LINK ProcessData"

# bugbug    dp_socketOption $newFile recvBuffer 63000
    dp_socketOption $server recvBuffer 252000
    dp_socketOption $newFile autoClose yes
    dp_socketOption $newFile keepAlive yes
    dp_after 10 "itcl_context $this ::LINK startThread $newFile"

    dp_socketOption $server nodelay 1
    dp_socketOption $newFile nodelay 1
    set link $newFile
    puts $newFile "Connection accepted"
    set state up
    database query "UPDATE links SET state='up' WHERE name='$name'"
}

#
# Shut down a connection by telling the other end to shutdown and
# removing the filehandler on this file.
#

body LINK::ShutdownLINK {file} {
    dalogmsg "WARN" "removing link $this (socket $file)"
    database query "UPDATE links SET state='down' WHERE name='$name'"
    set state down
    catch "dp_filehandler $file"
    dp_atexit delete "close $file"
    catch "dp_atclose $file clear"
    dp_atclose $server delete "itcl_context $this ::LINK ShutdownLINK $file"
    set link ""
}
 
}
