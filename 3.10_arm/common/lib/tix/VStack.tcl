# VStack.tcl --
#
#	virtual base class. Do not instantiate.  This is the core
#	class for all NoteBook style widgets. Stack maintains a list
#	of windows. It provides methods to create, delete windows as
#	well as stepping through them.
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
#


tixWidgetClass tixVStack {
    -classname TixVStack
    -superclass tixPrimitive
    -method {
	add delete pageconfigure pagecget pages raise raised
    }
    -flag {
	-dynamicgeometry -ipadx -ipady
    }
    -configspec {
	{-dynamicgeometry dynamicGeometry DynamicGeometry 0 tixVerifyBoolean}
	{-ipadx ipadX Pad 0}
	{-ipady ipadY Pad 0}
    }
}

proc tixVStack::InitWidgetRec {w} {
    upvar #0 $w data

    tixChainMethod $w InitWidgetRec

    set data(pad-x1) 0
    set data(pad-x2) 0
    set data(pad-y1) 0
    set data(pad-y2) 0

    set data(windows)  {}
    set data(nWindows) 0
    set data(topchild) {}

    set data(minW)   1
    set data(minH)   1

    set data(w:top)  $w
    set data(counter) 0
    set data(repack) 0
}

proc tixVStack::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings
    bind $w <Configure> "tixVStack::MasterGeomProc $w"
    bind $data(w:top) <Destroy> "tixVStack::DestroyTop $w"

    if {$data(repack) == 0} {
	set data(repack) 1
	tixWidgetDoWhenIdle tixCallMethod $w Resize
    }
}

#----------------------------------------------------------------------
# Public methods
#----------------------------------------------------------------------
proc tixVStack::add {w child args} {
    upvar #0 $w data

    set validOptions {-createcmd -raisecmd}

    set opt(-createcmd)  {}
    set opt(-raisecmd)   {}

    tixHandleOptions -nounknown opt $validOptions $args

    set data($child,raisecmd)  $opt(-raisecmd)
    set data($child,createcmd) $opt(-createcmd)

    set data(w:$child) [frame $data(w:top).$child]
    tixManageGeometry $data(w:$child) "tixVStack::ClientGeomProc $w"
    bind $data(w:$child) <Configure> \
	"tixVStack::ClientGeomProc $w -configure $data(w:$child)"
    bind $data(w:$child) <Destroy> "$w delete $child"

    lappend data(windows) $child
    incr data(nWindows) 1

    return $data(w:$child) 
}

proc tixVStack::delete {w child} {
    upvar #0 $w data

    if [info exists data($child,createcmd)] {
	if [winfo exists $data(w:$child)] {
	    bind $data(w:$child) <Destroy> {;}
	    destroy $data(w:$child)
	}
	catch {unset data($child,createcmd)}
	catch {unset data($child,raisecmd)}
	catch {unset data(w:$child)}

	set index [lsearch $data(windows) $child]
	if {$index >= 0} {
	    set data(windows) [lreplace $data(windows) $index $index]
	    incr data(nWindows) -1
	}

	if {$data(topchild) == $child} {
	    set data(topchild) {}
	    foreach page $data(windows) {
		if {$page != $child} {
		    $w raise $page
		    set data(topchild) $page
		    break
		}
	    }
	}
    } else {
	error "page $child does not exist"
    }
}

proc tixNoteBook::pagecget {w child option} {
    upvar #0 $w data

    if {![info exists data($child,createcmd)]} {
	error "page \"$child\" does not exist in $w"
    }

    case $option {
	-createcmd {
	    return "$data($child,createcmd)"
	}
	-raisecmd {
	    return "$data($child,raisecmd)"
	}
	default {
	    return [$data(w:top) pagecget $child $option]
	}
    }
}

proc tixNoteBook::pageconfigure {w child args} {
    upvar #0 $w data

    if {![info exists data($child,createcmd)]} {
	error "page \"$child\" does not exist in $w"
    }

    set len [llength $args]

    if {$len == 0} {
	set value [$data(w:top) pageconfigure $child]
	lappend value "-createcmd {} {} {} $data($child,createcmd)"
	lappend value "-raisecmd {} {} {} $data($child,raisecmd)"
	return $value
    }

    if {$len == 1} {
	case [lindex $args 0] {
	    -createcmd {
		return "-createcmd {} {} {} $data($child,createcmd)"
	    }
	    -raisecmd {
		return "-raisecmd {} {} {} $data($child,raisecmd)"
	    }
	    default {
		return [$data(w:top) pageconfigure $child [lindex $args 0]]
	    }
	}
    }

    # By default handle each of the options
    #
    set opt(-createcmd)  $data($child,createcmd)
    set opt(-raisecmd)   $data($child,raisecmd)

    tixHandleOptions -nounknown opt {-createcmd -raisecmd} $args

    #
    # the widget options
    set new_args {}
    tixForEach {flag value} $args {
	if {$flag != "-createcmd" && $flag != "-raisecmd"} {
	    lappend new_args $flag
	    lappend new_args $value
	}
    }

    if {[llength $new_args] >= 2} {
	eval $data(w:top) pageconfig $child $new_args
    }

    #
    # The add-on options
    set data($child,raisecmd)  $opt(-raisecmd)
    set data($child,createcmd) $opt(-createcmd)

    return {}
}

proc tixNoteBook::pages {w} {
    upvar #0 $w data

    return $data(windows)
}

proc tixVStack::raise {w child} {
    upvar #0 $w data

    if {![info exists data($child,createcmd)]} {
	error "page $child does not exist"
    }

    if {[info exists data($child,createcmd)] && $data($child,createcmd) !={}} {
	eval $data($child,createcmd)
	set data($child,createcmd) {}
    }

    # Hide the original visible window
    if {$data(topchild) != {} && $data(topchild) != $child} {
	tixUnmapWindow $data(w:$data(topchild))
    }

    set oldTopChild $data(topchild)
    set data(topchild) $child

    set myW [winfo width  $w]
    set myH [winfo height $w]

    set cW [expr $myW - $data(pad-x1) - $data(pad-x2) - 2*$data(-ipadx)]
    set cH [expr $myH - $data(pad-y1) - $data(pad-y2) - 2*$data(-ipady)]
    set cX [expr $data(pad-x1) + $data(-ipadx)]
    set cY [expr $data(pad-y1) + $data(-ipady)]

    if {$cW > 0 && $cH > 0} {
	tixMoveResizeWindow $data(w:$child) $cX $cY $cW $cH
	tixMapWindow $data(w:$child)
	raise $data(w:$child)
    }
    if {$oldTopChild != $child} {
	if {$data($child,raisecmd) != {}} {
 	    eval $data($child,raisecmd)
	}
    }
}


proc tixNoteBook::raised {w} {
    upvar #0 $w data
 
    return $data(topchild)
}
#----------------------------------------------------------------------
#
#
#----------------------------------------------------------------------
proc tixVStack::DestroyTop {w} {
    catch {
	destroy $w
    }
}

proc tixVStack::MasterGeomProc {w args} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    if {$data(repack) == 0} {
	set data(repack) 1
	tixWidgetDoWhenIdle tixCallMethod $w Resize
    }
}

proc tixVStack::ClientGeomProc {w flag client} {
    if {![winfo exists $w]} {
	return
    }
    upvar #0 $w data

    if {$data(repack) == 0} {
	set data(repack) 1
	tixWidgetDoWhenIdle tixCallMethod $w Resize
    }

    if {$flag == "-lostslave"} {
	error "Geometry Management Error: \
Another geometry manager has taken control of $client.\
This error is usually caused because a widget has been created\
in the wrong frame: it should have been created inside $client instead\
of $w"
    }
}

proc tixVStack::Resize {w} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    if {$data(nWindows) == 0} {
	set data(repack) 0
	return
    }

    if {$data(-width) == 0 || $data(-height) == 0} {
	if {!$data(-dynamicgeometry)} {
	    # Calculate my required width and height
	    #
	    set maxW 1
	    set maxH 1

	    foreach child $data(windows) {
		set cW [winfo reqwidth  $data(w:$child)]
		set cH [winfo reqheight $data(w:$child)]

		if {$maxW < $cW} {
		    set maxW $cW
		}
		if {$maxH < $cH} {
		    set maxH $cH
		}
	    }
	    set reqW $maxW
	    set reqH $maxH
	} else {
	    if {$data(topchild) != {}} {
		set reqW [winfo reqwidth  $data(w:$data(topchild))]
		set reqH [winfo reqheight $data(w:$data(topchild))]
	    } else {
		set reqW 1
		set reqH 1
	    }
	}

	incr reqW [expr $data(pad-x1) + $data(pad-x2) + 2*$data(-ipadx)]
	incr reqH [expr $data(pad-y1) + $data(pad-y2) + 2*$data(-ipady)]

	if {$reqW < $data(minW)} {
	    set reqW $data(minW)
	}
	if {$reqH < $data(minH)} {
	    set reqH $data(minH)
	}
    }
    # These take higher precedence
    #
    if {$data(-width)  != 0} {
	set reqW $data(-width)
    }
    if {$data(-height) != 0} {
	set reqH $data(-height)
    }

    if {[winfo reqwidth $w] != $reqW || [winfo reqheight $w] != $reqH} {
	if {![info exists data(counter)]} {
	    set data(counter) 0
	}
        if {$data(counter) < 50} {
            incr data(counter)
	    tixGeometryRequest $w $reqW $reqH
	    tixWidgetDoWhenIdle tixCallMethod $w Resize
	    set data(repack) 1
	    return
	}
    }
    set data(counter) 0

    if {$data(w:top) != $w} {
	tixMoveResizeWindow $data(w:top) 0 0 [winfo width $w] [winfo height $w]
	tixMapWindow $data(w:top)
    }

    if {$data(topchild) == {}} {
	set data(topchild) [lindex $data(windows) 0]
    }
    tixCallMethod $w raise $data(topchild)

    set data(repack) 0
}
