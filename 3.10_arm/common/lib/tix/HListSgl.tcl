# HList.tcl --
#
#	This file defines the default bindings for Tix Hierarchical Listbox
#	widgets.
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#



#--------------------------------------------------------------------------
# tkPriv elements used in this file:
#
# afterId -		Token returned by "after" for autoscanning.
# fakeRelease -		Cancel the ButtonRelease-1 after the user double click
#--------------------------------------------------------------------------
#
proc tixHListBindSingle {} {
    tixBind TixHListSingle <ButtonPress-1> {
	tixHListSingle::Button-1 %W %x %y
    }
    tixBind TixHListSingle <ButtonRelease-1> {
	tixHListSingle::ButtonRelease-1 %W %x %y
    }
    tixBind TixHListSingle <Double-ButtonPress-1> {
	tixHListSingle::Double-1 %W  %x %y
    }
    tixBind TixHListSingle <B1-Motion> {
	set tkPriv(x) %x 
	set tkPriv(y) %y
	set tkPriv(X) %X
	set tkPriv(Y) %Y

	tixHListSingle::B1-Motion %W %x %y
    }
    tixBind TixHListSingle <B1-Leave> {
	set tkPriv(x) %x 
	set tkPriv(y) %y
	set tkPriv(X) %X
	set tkPriv(Y) %Y

	tixHListSingle::B1-Leave %W
    }
    tixBind TixHListSingle <B1-Enter> {
	tixHListSingle::B1-Enter %W %x %y
    }

    # Keyboard bindings
    #
    tixBind TixHListSingle <Up> {
	tixHListSingle::UpDown %W prev
    }
    tixBind TixHListSingle <Down> {
	tixHListSingle::UpDown %W next
    }
    tixBind TixHListSingle <Left> {
	tixHListSingle::LeftRight %W left
    }
    tixBind TixHListSingle <Right> {
	tixHListSingle::LeftRight %W right
    }
    tixBind TixHListSingle <Prior> {
	%W yview scroll -1 pages
    }
    tixBind TixHListSingle <Next> {
	%W yview scroll 1 pages
    }
    tixBind TixHListSingle <Return> {
	tixHListSingle::Keyboard-Activate %W 
    }
    tixBind TixHListSingle <space> {
	tixHListSingle::Keyboard-Browse %W 
    }
}



#----------------------------------------------------------------------
#
#
#			 Mouse bindings
#
#
#----------------------------------------------------------------------


proc tixHListSingle::Button-1 {w x y} {
#    if {[$w cget -state] == "disabled"} {
#	return
#    }

    if [$w cget -takefocus] {
	focus $w
    }

    case [tixHListSingle::GetState $w] {
	{0} {
	    # %% Check if not on title
	    tixHListSingle::GoState 1 $w $x $y

	}
    }
}

proc tixHListSingle::DragTimer {w ent} {
    case [tixHListSingle::GetState $w] {
	{1} {
	    # fire up
	}
    }
}


proc tixHListSingle::ButtonRelease-1 {w x y} {
    case [tixHListSingle::GetState $w] {
	{5 16} {
	    tixHListSingle::GoState 6 $w $x $y
	}
	{15} {
	    tixHListSingle::GoState 17 $w $x $y
	}
	{10 11} {
	    tixHListSingle::GoState 18 $w
	}
	{13 20} {
	    tixHListSingle::GoState 14 $w $x $y
	}
	{21} {
	    tixHListSingle::GoState 22 $w
	}
    }
}

proc tixHListSingle::Double-1 {w x y} {
    case [tixHListSingle::GetState $w] {
	{0} {
	    tixHListSingle::GoState 23 $w $x $y
	}
    }
}

proc tixHListSingle::B1-Motion {w x y} {
    case [tixHListSingle::GetState $w] {
	{1} {
	    tixHListSingle::GoState 5 $w $x $y 
	}
	{5 16} {
	    tixHListSingle::GoState 5 $w $x $y 
	}
	{13 20 21} {
	    tixHListSingle::GoState 20 $w $x $y 
	}
    }
}

proc tixHListSingle::B1-Leave {w} {
    case [tixHListSingle::GetState $w] {
	{5} {
	    tixHListSingle::GoState 10 $w
	}
    }
}

proc tixHListSingle::B1-Enter {w x y} {
    case [tixHListSingle::GetState $w] {
	{10 11} {
	    tixHListSingle::GoState 12 $w $x $y
	}
    }
}

proc tixHListSingle::AutoScan {w} {
    case [tixHListSingle::GetState $w] {
	{10 11} {
	    tixHListSingle::GoState 11 $w
	}
    }
}

#----------------------------------------------------------------------
#
#
#			 Key bindings
#
#
#----------------------------------------------------------------------
proc tixHListSingle::Keyboard-Activate {w} {
    if {[tixHListSingle::GetState $w] != 0} {
	return
    }
    set ent [$w info anchor]

    if {$ent == ""} {
	return
    }

    if {[$w cget -selectmode] == "single"} {
	$w select clear
	$w select set $ent
    }

    set command [$w cget -command]
    if {$command != {}} {
	set bind(specs) {%V}
	set bind(%V)    $ent

	tixEvalCmdBinding $w $command bind $ent
    }
}

proc tixHListSingle::Keyboard-Browse {w} {
    if {[tixHListSingle::GetState $w] != 0} {
	return
    }
    set ent [$w info anchor]

    if {$ent == ""} {
	return
    }

    if {[$w cget -selectmode] == "single"} {
	$w select clear
	$w select set $ent
    }

    tixHListSingle::Browse $w $ent
}

proc tixHListSingle::LeftRight {w spec} {
    if {[tixHListSingle::GetState $w] != 0} {
	return
    }

    set anchor [$w info anchor]
    if {$anchor == ""} {
	set anchor [lindex [$w info children] 0]
    }

    set ent $anchor
    while {1} {
	set e $ent
	if {$spec == "left"} {
	    set ent [$w info parent $e]

	    if {$ent == {} || [$w entrycget $ent -state] == "disabled"} {
		set ent [$w info prev $e]
	    }
	} else {
	    set ent [lindex [$w info children $e] 0]

	    if {$ent == {} || [$w entrycget $ent -state] == "disabled"} {
		set ent [$w info next $e]
	    }
	}

	if {$ent == {}} {
	    break
	}
 	if {[$w entrycget $ent -state] == "disabled"} {
	    continue
	}
 	if [$w info hidden $ent] {
	    continue
	}
	break
    }

    if {$ent == {}} {
       return
    }

    $w anchor set $ent
    $w see $ent

    if {[$w cget -selectmode] != "single"} {
	$w select clear
	$w selection set $ent

	tixHListSingle::Browse $w $ent
    }
}

proc tixHListSingle::UpDown {w spec} {
    if {[tixHListSingle::GetState $w] != 0} {
	return
    }
    set anchor [$w info anchor]

    if {$anchor == ""} {
	set anchor [lindex [$w info children] 0]

	if {$anchor == {}} {
	    return
	}

	if {[$w entrycget $anchor -state] != "disabled"} {
	    # That's a good anchor
	    set done 1
	} else {
	    # We search for the first non-disabled entry (downward)
	    set spec next
	}
    }

    set ent $anchor
    set done 0
    # Find the prev/next non-disabled entry
    #
    while {!$done} {
	set ent [$w info $spec $ent]
	if {$ent == {}} {
	    break
	}
	if {[$w entrycget $ent -state] == "disabled"} {
	    continue
	}
	if [$w info hidden $ent] {
	    continue
	}
	break
    }

    if {$ent == {}} {
	return
    } else {
	$w anchor set $ent
	$w see $ent

	if {[$w cget -selectmode] != "single"} {
	    $w select clear
	    $w selection set $ent

	    tixHListSingle::Browse $w $ent
	}
    }
}

proc tixHListSingle::GetNearest {w y} {
    set ent [$w nearest $y]

    if {$ent != {}} {
	if {[$w entrycget $ent -state] == "disabled"} {
	    return {}
	}
    }

    return $ent
}

#----------------------------------------------------------------------
#
#			STATE MANIPULATION
#
#
#----------------------------------------------------------------------
proc tixHListSingle::GetState {w} {
    global $w:priv:state

    if {![info exists $w:priv:state]} {
	set $w:priv:state 0
    }
    return [set $w:priv:state]
}

proc tixHListSingle::SetState {w n} {
    global $w:priv:state

    set $w:priv:state $n
}

proc tixHListSingle::GoState {n w args} {

#    puts "going from [tixHListSingle::GetState $w] --> $n"

    tixHListSingle::SetState $w $n
    eval tixHListSingle::GoState-$n $w $args
}

#----------------------------------------------------------------------
#			States
#----------------------------------------------------------------------
proc tixHListSingle::GoState-0 {w} {
}
proc tixHListSingle::GoState-1 {w x y} {
    set oldEnt [$w info anchor]
    set ent [tixHListSingle::SetAnchor $w $x $y]

    # %% add if is in dragdrop mode
    set dragdrop 0

    if {$ent == {}} {
	tixHListSingle::GoState 0 $w
    }

    set info [$w info item $x $y]
    if {[lindex $info 1] == "indicator"} {
	tixHListSingle::GoState 13 $w $ent $oldEnt
    } elseif {$dragdrop} {
	tixHListSingle::GoState 15 $w
    } else {
	tixHListSingle::GoState 16 $w $ent
    }
}

proc tixHListSingle::GoState-5 {w x y} {
    set oldEnt [$w info anchor]

    set ent [tixHListSingle::SetAnchor $w $x $y]

    if {$ent == {} || $oldEnt == $ent} {
	return
    }

    if {[$w cget -selectmode] != "single"} {
	tixHListSingle::Select $w $ent
	tixHListSingle::Browse $w $ent
    }
}

proc tixHListSingle::GoState-6 {w x y} {
    set ent [tixHListSingle::SetAnchor $w $x $y]

    if {$ent == {}} {
	tixHListSingle::GoState 0 $w
	return
    }
    tixHListSingle::Select $w $ent
    tixHListSingle::Browse $w $ent

    tixHListSingle::GoState 0 $w
}

proc tixHListSingle::GoState-10 {w} {
    tixHListSingle::StartScan $w
}

proc tixHListSingle::GoState-11 {w} {
    global tkPriv

    tixHListSingle::DoScan $w

    set oldEnt [$w info anchor]
    set ent [tixHListSingle::SetAnchor $w $tkPriv(x) $tkPriv(y)]

    if {$ent == {} || $oldEnt == $ent} {
	return
    }

    if {[$w cget -selectmode] != "single"} {
	tixHListSingle::Select $w $ent
	tixHListSingle::Browse $w $ent
    }
}

proc tixHListSingle::GoState-12 {w x y} {
    tkCancelRepeat
    tixHListSingle::GoState 5 $w $x $y 
}

proc tixHListSingle::GoState-13 {w ent oldEnt} {
    global tkPriv
    set tkPriv(tix,indicator) $ent
    set tkPriv(tix,oldEnt)    $oldEnt
    tixHListSingle::IndicatorCmd $w <Arm> $ent
}

proc tixHListSingle::GoState-14 {w x y} {
    global tkPriv

    if [tixHListSingle::InsideArmedIndicator $w $x $y] {
	$w anchor set $tkPriv(tix,indicator)
	$w select clear
	$w select set $tkPriv(tix,indicator)
	tixHListSingle::IndicatorCmd $w <Activate> $tkPriv(tix,indicator)
    } else {
	tixHListSingle::IndicatorCmd $w <Disarm>   $tkPriv(tix,indicator)
    }

    unset tkPriv(tix,indicator)
    tixHListSingle::GoState 0 $w
}

proc tixHListSingle::GoState-16 {w ent} {
    if {$ent == {}} {
	return
    }
    if {[$w cget -selectmode] != "single"} {
	tixHListSingle::Select $w $ent
	tixHListSingle::Browse $w $ent
    }
}

proc tixHListSingle::GoState-18 {w} {
    global tkPriv
    tkCancelRepeat
    tixHListSingle::GoState 6 $w $tkPriv(x) $tkPriv(y) 
}

proc tixHListSingle::GoState-20 {w x y} {
    global tkPriv

    if {![tixHListSingle::InsideArmedIndicator $w $x $y]} {
	tixHListSingle::GoState 21 $w $x $y
    } else {
	tixHListSingle::IndicatorCmd $w <Arm> $tkPriv(tix,indicator)
    }
}

proc tixHListSingle::GoState-21 {w x y} {
    global tkPriv

    if {[tixHListSingle::InsideArmedIndicator $w $x $y]} {
	tixHListSingle::GoState 20 $w $x $y
    } else {
	tixHListSingle::IndicatorCmd $w <Disarm> $tkPriv(tix,indicator)
    }
}

proc tixHListSingle::GoState-22 {w} {
    global tkPriv

    if {$tkPriv(tix,oldEnt) != {}} {
	$w anchor set $tkPriv(tix,oldEnt)
    } else {
	$w anchor clear
    }
    tixHListSingle::GoState 0 $w
}

proc tixHListSingle::GoState-23 {w x y} {
    set ent [tixHListSingle::GetNearest $w $y]

    if {$ent != {}} {
	set info [$w info item $x $y]

	if {[lindex $info 1] == "indicator"} {
	    tixHListSingle::IndicatorCmd $w <Activate> $ent
	} else {
	    $w select set $ent
	    set command [$w cget -command]
	    if {$command != {}} {
		set bind(specs) {%V}
		set bind(%V)    $ent

		tixEvalCmdBinding $w $command bind $ent
	    }
	}
    }
    tixHListSingle::GoState 0 $w
}


#----------------------------------------------------------------------
#			callback actions
#----------------------------------------------------------------------
proc tixHListSingle::SetAnchor {w x y} {
    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent != {} && [$w entrycget $ent -state] != "disabled"} {
	$w anchor set $ent
	$w see $ent
	return $ent
    }

    return {}
}

proc tixHListSingle::Select {w ent} {
    $w selection clear
    $w select set $ent
}

proc tixHListSingle::StartScan {w} {
    global tkPriv
    set tkPriv(afterId) [after 50 tixHListSingle::AutoScan $w]
}

proc tixHListSingle::DoScan {w} {
    global tkPriv
    set x $tkPriv(x)
    set y $tkPriv(y)
    set X $tkPriv(X)
    set Y $tkPriv(Y)

    if {$y >= [winfo height $w]} {
	$w yview scroll 1 units
    } elseif {$y < 0} {
	$w yview scroll -1 units
    } elseif {$x >= [winfo width $w]} {
	$w xview scroll 2 units
    } elseif {$x < 0} {
	$w xview scroll -2 units
    } else {
	return
    }

    set tkPriv(afterId) [after 50 tixHListSingle::AutoScan $w]
}

proc tixHListSingle::IndicatorCmd {w event ent} {
    set cmd [$w cget -indicatorcmd]

    if {$cmd != {}} {
	global tixPriv
	set tixPriv(b:event) $event
	set bind(specs) {%V}
	set bind(%V)    $ent

	tixEvalCmdBinding $w $cmd bind $ent
    }
}

proc tixHListSingle::InsideArmedIndicator {w x y} {
    global tkPriv

    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent == {} || $ent != $tkPriv(tix,indicator)} {
	return 0
    }

    set info [$w info item $x $y]
    if {[lindex $info 1] == "indicator"} {
	return 1
    } else {
	return 0
    }
}

proc tixHListSingle::Browse {w ent} {
    set browsecmd [$w cget -browsecmd]
    if {$browsecmd != {}} {
	set bind(specs) {%V}
	set bind(%V)    $ent

	tixEvalCmdBinding $w $browsecmd bind $ent
    }
}
#----------------------------------------------------------------------
#
#		    Drag + Drop Bindings
#
#----------------------------------------------------------------------

	     #----------------------------------------#
	     #	          Sending Actions	      #
	     #----------------------------------------#

#----------------------------------------------------------------------
#  tixHListSingle::Send:WaitDrag --
#
#	Sender wait for dragging action
#----------------------------------------------------------------------
proc tixHListSingle::Send:WaitDrag {w x y} {
    global tixPriv

    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent != {}} {
	$w anchor set $ent
	$w select clear
	$w select set $ent
 
	set tixPriv(dd,$w:moved) 0
	set tixPriv(dd,$w:entry) $ent

#	set browsecmd [$w cget -browsecmd]
#	if {$browsecmd != {} && $ent != {}} {
#	    eval $browsecmd $ent
#	}
    }
}

proc tixHListSingle::Send:StartDrag {w x y} {
    global tixPriv
    set dd [tixGetDragDropContext $w]

    if {![info exists tixPriv(dd,$w:entry)]} {
	return
    }
    if {$tixPriv(dd,$w:entry) == {}} {
	return
    }

    if {$tixPriv(dd,$w:moved) == 0} {
	$w dragsite set $tixPriv(dd,$w:entry)
	set tixPriv(dd,$w:moved) 1
	$dd config -source $w -command "tixHListSingle::Send:Cmd $w"
	$dd startdrag $X $Y
    } else {
	$dd drag $X $Y
    }
}

proc tixHListSingle::Send:DoneDrag {w x y} {
    global tixPriv
    global moved

    if {![info exists tixPriv(dd,$w:entry)]} {
	return
    }
    if {$tixPriv(dd,$w:entry) == {}} {
	return
    }

    if {$tixPriv(dd,$w:moved) == 1} {
	set dd [tixGetDragDropContext $w]
	$dd drop $X $Y
    }
    $w dragsite clear
    catch {unset tixPriv(dd,$w:moved)}
    catch {unset tixPriv(dd,$w:entry)}
}

proc tixHListSingle::Send:Cmd {w option args} {
    set dragCmd [$w cget -dragcmd]
    if {$dragCmd != {}} {
	return [eval $dragCmd $option $args]
    }

    # Perform the default action
    #
    case "$option" {
	who {
	    return $w
	}
	types {
	    return {data text}
	}
	get {
	    global tixPriv
	    if {[lindex $args 0] == "text"} {
		if {$tixPriv(dd,$w:entry) != {}} {
		    return [$w entrycget $tixPriv(dd,$w:entry) -text]
		}
	    }
	    if {[lindex $args 0] == "data"} {
		if {$tixPriv(dd,$w:entry) != {}} {
		    return [$w entrycget $tixPriv(dd,$w:entry) -data]
		}
	    }
	}
    }
}

	     #----------------------------------------#
	     #	          Receiving Actions	      #
	     #----------------------------------------#
proc tixHListSingle::Rec:DragOver {w sender x y} {
    if {[$w cget -selectmode] != "dragdrop"} {
	return
    }

    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent != {}} {
	$w dropsite set $ent
    } else {
	$w dropsite clear
    }
}

proc tixHListSingle::Rec:DragIn {w sender x y} {
    if {[$w cget -selectmode] != "dragdrop"} {
	return
    }
    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent != {}} {
	$w dropsite set $ent
    } else {
	$w dropsite clear
    }
}

proc tixHListSingle::Rec:DragOut {w sender x y} {
    if {[$w cget -selectmode] != "dragdrop"} {
	return
    }
    $w dropsite clear
}

proc tixHListSingle::Rec:Drop {w sender x y} {
    if {[$w cget -selectmode] != "dragdrop"} {
	return
    }
    $w dropsite clear

    set ent [tixHListSingle::GetNearest $w $y]
    if {$ent != {}} {
	$w anchor set $ent
	$w select clear
	$w select set $ent
    }
 
    set dropCmd [$w cget -dropcmd]
    if {$dropCmd != {}} {
	eval $dropCmd $sender $x $y
	return
    }

#    set browsecmd [$w cget -browsecmd]
#    if {$browsecmd != {} && $ent != {}} {
#	eval $browsecmd [list $ent]
#    }
}

tixDropBind TixHListSingle <In>   "tixHListSingle::Rec:DragIn %W %S %x %y"
tixDropBind TixHListSingle <Over> "tixHListSingle::Rec:DragOver %W %S %x %y"
tixDropBind TixHListSingle <Out>  "tixHListSingle::Rec:DragOut %W %S %x %y"
tixDropBind TixHListSingle <Drop> "tixHListSingle::Rec:Drop %W %S %x %y"
