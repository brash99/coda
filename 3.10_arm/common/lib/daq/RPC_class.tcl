# RPC_class.tcl --
#
# Methods to create reentrant RPC servers (full duplex) 
#
########################################################################
if {"[info command RPC]"!="RPC"} {
global tcl_modules

lappend tcl_modules "RPC_class.tcl {} {} {\$Id: RPC_class.tcl,v 1.5 1998/05/27 13:45:02 heyes Exp $}"
class RPC {
    private variable Acl {}
    private variable dp_server_file {}
    private method test_cmd {} {puts hello}
    private method AcceptRPCConnection {loginFunc checkCmd status file} {}
    private method  CheckHost {inetAddr} {}
    private method  ShutdownServer {} {}
    private method  ShutdownRPC {file} {}
    private method  CloseRPCFile {} {}

    private method  CloseRPC {file} {}
    private method  CleanupRPC {file} {}
    private method  ns_connection {file} {}

    method  Access {str} {}
    constructor {{server 1} {port 0} {loginFunc none} {checkCmd none} {retFile 0}} {}
}

#
# Access control lists -- sort of an xhost style implementation
#

body RPC::constructor  {{serv 1} {port 0} {loginFunc none} {checkCmd none} {retFile 0}} {
    if {!$serv} return
    set rv [dp_connect -server $port -reuseAddr]
    set dp_server_file [lindex $rv 0]
    
    set port [lindex $rv 1]
    dp_SetNsCmd $dp_server_file
    dp_filehandler $dp_server_file re "itcl_context $this ::RPC AcceptRPCConnection $loginFunc $checkCmd"
    dp_atexit appendUnique "close $dp_server_file"
    dp_atclose $dp_server_file append "itcl_context $this ::RPC ShutdownServer"

    if ![catch "database get databases"] {
	global env
	set name [info namespace tail $this]
	set host [exec hostname]
	if {[catch "database set database $env(EXPID)" res] } {
	    puts "ERROR : database set returned \"$res\" "
	    exit
	}
	catch "database query \"SELECT name FROM process WHERE name='$name'\" " res
	if [database get rows] {
	    set res [database query "UPDATE process SET host='$host',port=$port WHERE name='$name'"]
	} else {
	    set res [database query "INSERT INTO process (name,host,port) VALUES ('$name','$host',$port)"]
	}
    } else {
	puts "no \"database command\""
    }

    return $port
}

body RPC::Access {str} {
    global Acl
    set key [string range $str 0 0]
    set str [string range $str 1 end]
    case $key in {
    "+" {
	    if {[string length $str] == 0} {
		set Acl {}
		set rv {Access control disabled. Any clients may connect}
	    } else {
		if {([string first * $str] != -1) ||
		    ([string first \[ $str] != -1) ||
		    ([string first ? $str] != -1) ||
		    ([string first \] $str] != -1)} {
		    set rv "Clients from $str may connect"
		    set Acl [Acl+ Acl $str]
		} else {
		    set addr [dp_address create $str 0]
		    set ipaddr [lindex [dp_address info $addr] 0]
		    dp_address delete $addr
		    if {$ipaddr == "255.255.255.255"} {
			error "Unknown host $str"
		    }
		    set Acl [Acl+ Acl $ipaddr]
		    set rv "Clients from $ipaddr may connect"
		}
	    }
	}

    "-" {
	    if {[string length $str] == 0} {
		set Acl {{0 *} {0 *}}
		set rv {Access control enabled. No clients may connect}
	    } else {
		if {([string first * $str] != -1) ||
		    ([string first \[ $str] != -1) ||
		    ([string first ? $str] != -1) ||
		    ([string first \] $str] != -1)} {
		    set rv "Clients from $str may not connect"
		    set Acl [Acl- Acl $str]
		} else {
		    set addr [dp_address create $str 0]
		    set ipaddr [lindex [dp_address info $addr] 0]
		    dp_address delete $addr
		    if {$ipaddr == "255.255.255.255"} {
			error "Unknown host $str"
		    }
		    set Acl [Acl- Acl $ipaddr]
		    set rv "Clients from $ipaddr may not connect"
		}
	    }
	}

	default {global Acl; return $Acl}
    }
    return $rv
}

########################################################################

body RPC::CheckHost {inetAddr} {
    if {[AclCheck Acl $inetAddr] == 0} {
	error "Host not authorized"
    }
}

body RPC::AcceptRPCConnection {loginFunc checkCmd status file} {
    if {[string compare $status e] == 0} {
	close $file;
	return; 
    }
    set connection [dp_accept $file]
    set newFile [lindex $connection 0]
    set inetAddr [lindex $connection 1]
    if {[string compare "none" $loginFunc] != 0} {
	set error [catch {eval $loginFunc $inetAddr} msg]
	if $error {
	    puts $newFile "Connection refused: $msg"
	    close $newFile
	    return;
	}
    }
    puts $newFile "Connection accepted"
    CleanupRPC $newFile
    dp_filehandler $newFile r "dp_ProcessRPCCommand $dp_server_file"
    dp_SetCheckCmd $newFile $checkCmd
}

########################################################################
#
# Shut down the listening socket.  This is usually invoked as an
# atclose callback.  It arranges to delete the filehandler once all
# processing has been done.
#
body RPC::ShutdownServer {} {
    dp_shutdown $dp_server_file both
    dp_filehandler $dp_server_file
    catch {dp_atexit delete "close $dp_server_file"}
}

#
# Shut down a connection by telling the other end to shutdown and
# removing the filehandler on this file.
#
# Step 1: remove the file handler to prevent evaluating any new RPCs
# Step 2: Send an RDO to the far end to shutdown the connection
# Step 3: Clean up the call to shutdown the connection on exit.
#
body RPC::ShutdownRPC {file} {
    dp_filehandler $file
    dp_RDO $file "itcl_context \$this ::RPC CloseRPCFile"
    dp_atexit delete "close $file"
}

#
# Close an RPC file:  shut down the connection and do the real close
#
body RPC::CloseRPC {file} {
    ShutdownRPC $file
    close $file
}

body RPC::CleanupRPC {file} {
    dp_atclose $file appendUnique "itcl_context $this ::RPC ShutdownRPC $file"
    dp_atexit appendUnique "close $file"
}

########################################################################
#
# Respond to remote sites request to close the rpc file.
# In this case, we don't want to call dp_ShutdownRPC (which will,
# in turn, try to close the remote site which is already closed),
# so we need to remove the dp_ShutdownRPC call from the atclose
# callback list before calling close.
#

body RPC::CloseRPCFile {} {
    global rpcFile
    dp_atclose $rpcFile delete "itcl_context $this ::RPC ShutdownRPC $rpcFile"
    dp_filehandler $rpcFile
    dp_atexit delete "close $rpcFile"
    close $rpcFile
}

}
