# Util.tcl --
#
#	Handles the event bindings of the -command and -browsecmd options
#	(and various of others such as -validatecmd).
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#----------------------------------------------------------------------
# Evaluate high-level bindings (-command, -browsecmd, etc):
# with % subsitution or without (compatibility mode)
#
#
# BUG : if a -command is intercepted by a hook, the hook must use
#       the same record name as the issuer of the -command. For the time
#	being, you must use the name "bind" as the record name!!!!!
#
#----------------------------------------------------------------------
set _tix_event_flags ""
append _tix_event_flags " %%"
append _tix_event_flags " %#"
append _tix_event_flags " %a"
append _tix_event_flags " %b"
append _tix_event_flags " %c"
append _tix_event_flags " %d"
append _tix_event_flags " %f"
append _tix_event_flags " %h"
append _tix_event_flags " %k"
append _tix_event_flags " %m"
append _tix_event_flags " %o"
append _tix_event_flags " %p"
append _tix_event_flags " %s"
append _tix_event_flags " %t"
append _tix_event_flags " %w"
append _tix_event_flags " %x"
append _tix_event_flags " %y"
append _tix_event_flags " %A"
append _tix_event_flags " %B"
append _tix_event_flags " %E"
append _tix_event_flags " %K"
append _tix_event_flags " %N"
append _tix_event_flags " %R"
append _tix_event_flags " %S"
append _tix_event_flags " %T"
append _tix_event_flags " %W"
append _tix_event_flags " %X"
append _tix_event_flags " %Y"

proc tixBind {tag event action} {
    global _tix_event_flags

    set cmd ""
    append cmd "set tixPriv(b:event) $event; "
    append cmd "_tixRecordFlags $_tix_event_flags; "
    append cmd "$action; "
    append cmd "catch {unset _tix_event_sub}; "
    append cmd "catch {unset tixPriv(b:event)}; "

    bind $tag $event $cmd
}

proc _tixRecordFlags $_tix_event_flags {
    global _tix_event_flags _tix_event_sub

    foreach f $_tix_event_flags {
	set _tix_event_sub($f) [set $f]
    }
}

proc tixEvent {option args} {
    global tixPriv  _tix_event_sub

    case $option {
	type {
	    return $tixPriv(b:event)
	}
	flag {
	    set f %[lindex $args 0]
	    if [info exists _tix_event_sub($f)] {
		return $_tix_event_sub($f)
	    }
	    error "The flag \"$flag\" does not exist"
	}
	match {
	    return [string match [lindex $args 0] $tixPriv(b:event)]
	}
	default {
	    error "unknown option \"$option\""
	}
    }
}

proc tixEvalCmdBinding {w cmd {subst {}} args} {
    global tixPriv _tix_event_flags _tix_event_sub tix

    if {![info exists tixPriv(b:event)]} {
	set tixPriv(b:event) <Application>
    }

    if {$subst != ""} {
	upvar $subst bind
	foreach spec $bind(specs) {
	    set _tix_event_sub($spec) $bind($spec)
	}
    }

    if [tixGetBoolean -nocomplain $tix(-extracmdargs)] {
	# Compatibility mode
	#
	eval $cmd $args
    } else {
	eval $cmd
    }
}
