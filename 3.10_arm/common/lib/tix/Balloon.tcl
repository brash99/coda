# Balloon.tcl --
#
#	The help widget. It provides both "balloon" type of help
#	message and "status bar" type of help message. You can use
#	this widget to indicate the function of the widgets inside
#	your application.
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


tixWidgetClass tixBalloon {
    -classname TixBalloon
    -superclass tixShell
    -method {
	bind post unbind
    }
    -flag {
	-installcolormap -initwait -state -statusbar
    }
    -configspec {
	{-installcolormap installColormap InstallColormap false}
	{-initwait initWait InitWait 1000}
	{-state state State both}
	{-statusbar statusBar StatusBar {}}

 	{-cursor cursor Cursur left_ptr}
    }
    -default {
	{*background 			#ffff60}
	{*foreground 			black}
	{*borderWidth 			0}
	{.borderWidth 			1}
	{.background 			black}
	{*Label.anchor			w}
	{*Label.justify			left}
    }
}

# Class Record
#
set tixBalloon(bals) {}

proc tixBalloon::InitWidgetRec {w} {
    upvar #0 $w data
    global tixBalloon

    tixChainMethod $w InitWidgetRec

    set data(isActive)    0
    set data(client)    {}

    lappend tixBalloon(bals) $w
}

proc tixBalloon::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget

    wm overrideredirect $w 1
    wm withdraw $w

    # Frame 1 : arrow
    frame $w.f1 -bd 0
    set data(w:label) [label $w.f1.lab -bd 0 -relief flat \
		       -bitmap [tix getbitmap balArrow]]
    pack $data(w:label) -side left -padx 1 -pady 1
    
    # Frame 2 : Message
    frame $w.f2 -bd 0
    set data(w:message) [label $w.f2.message -padx 0 -pady 0 -bd 0]
    pack $data(w:message) -side left -expand yes -fill both -padx 10 -pady 1

    # Pack all
    pack $w.f1 -fill both
    pack $w.f2 -fill both    

    # This is an event tag used by the clients
    #
    bind TixBal$w <Destroy> "tixBalloon::ClientDestroy $w %W"
}

proc tixBalloon::Destructor {w} {
    global tixBalloon

    set bals {}
    foreach b $tixBalloon(bals) {
	if {$w != $b} {
	    lappend bals $b
	}
    }
    set tixBalloon(bals) $bals
}

#----------------------------------------------------------------------
# Config:
#----------------------------------------------------------------------
proc tixBalloon::config-state {w value} {
    upvar #0 $w data

    case $value {
	{none balloon status both} {}
	default {
	   error "invalid value $value, must be none, balloon, status, or both"
	}
    }
}

#----------------------------------------------------------------------
# "RAW" event bindings:
#----------------------------------------------------------------------

bind all <B1-Motion> 	    "+tixBalloon:XXMotion %X %Y 1"
bind all <B2-Motion> 	    "+tixBalloon:XXMotion %X %Y 2"
bind all <B3-Motion> 	    "+tixBalloon:XXMotion %X %Y 3"
bind all <B4-Motion> 	    "+tixBalloon:XXMotion %X %Y 4"
bind all <B5-Motion> 	    "+tixBalloon:XXMotion %X %Y 5"
bind all <Any-Motion> 	    "+tixBalloon:XXMotion %X %Y 0"
bind all <Leave>      	    "+tixBalloon:XXMotion %X %Y %b"
bind all <Button>      	    "+tixBalloon:XXButton   %X %Y %b"
bind all <ButtonRelease>    "+tixBalloon:XXButtonUp %X %Y %b"

proc tixBalloon:XXMotion {rootX rootY b} {
    global tixBalloon

    foreach w $tixBalloon(bals) {
	tixBalloon::XXMotion $w $rootX $rootY $b
    }
}

proc tixBalloon:XXButton {rootX rootY b} {
    global tixBalloon

    foreach w $tixBalloon(bals) {
	tixBalloon::XXButton $w $rootX $rootY $b
    }
}

proc tixBalloon:XXButtonUp {rootX rootY b} {
    global tixBalloon

    foreach w $tixBalloon(bals) {
	tixBalloon::XXButtonUp $w $rootX $rootY $b
    }
}


# return true if d is a descendant of w
#
proc tixIsDescendant {w d} {
    if [string match $w .] {
	return 1
    }
    return [string match $w.* $d]
}

# All the button events are fine if the ballooned widget is
# a descendant of the grabbing widget
#
proc tixBalloon::GrabBad {w cw} {
    global tixBalloon

    set g [grab current $w]
    if {$g == {}} {
	return 0
    }
    if [info exists tixBalloon(g_ignore,$g)] {
	return 1
    }
    if [info exists tixBalloon(g_ignore,[winfo class $g])] {
	return 1
    }
    if {$g == $cw || [tixIsDescendant $g $cw]} {
	return 0
    }
    return 1
}

proc tixBalloon::XXMotion {w rootX rootY b} {
    upvar #0 $w data

    if {$data(-state) == "none"} {
	return
    }

    if {$b == 0} {
	catch {unset data(b:1)}
	catch {unset data(b:2)}
	catch {unset data(b:3)}
	catch {unset data(b:4)}
	catch {unset data(b:5)}
    }


    if {[array names data b:*] != {}} {
	# Some buttons are down. Do nothing
	#
	return
    }

    set cw [winfo containing $rootX $rootY]
    if [tixBalloon::GrabBad $w $cw] {
	return
    }

    # Find the a client window that matches
    #
    if {$w == $cw || [string match $w.* $cw]} {
	# Cursor moved over the balloon -- Ignore
	return
    }

    while {$cw != {}} {
	if [info exists data(m:$cw)] {
	    set client $cw
	    break
	} else {
	    set cw [winfo parent $cw]
	}
    }
    if {![info exists client]} {
	# The cursor is at a position covered by a non-client
	# Popdown the balloon if it is up
	if {$data(isActive)} {
	    tixBalloon::Deactivate $w
	}
	set data(client) {}
	catch {
	    unset data(cancel) 
	}
	return
    }

    if {$data(client) != $client} {
	if {$data(isActive)} {
	    tixBalloon::Deactivate $w
	}
	set data(client) $client
	after $data(-initwait) tixBalloon::SwitchToClient $w $client
    }
}

proc tixBalloon::XXButton {w rootX rootY b} {
    upvar #0 $w data

    tixBalloon::XXMotion $w $rootX $rootY $b

    set data(b:$b) 1

    if {$data(isActive)} {
	tixBalloon::Deactivate $w
    } else {
	set data(cancel) 1
    }
}

proc tixBalloon::XXButtonUp {w rootX rootY b} {
    upvar #0 $w data

    tixBalloon::XXMotion $w $rootX $rootY $b
    catch {
	unset data(b:$b)
    }
}

#----------------------------------------------------------------------
# "COOKED" event bindings:
#----------------------------------------------------------------------

# switch the balloon to a new client
#
proc tixBalloon::SwitchToClient {w client} {
    upvar #0 $w data

    if {![winfo exists $w]} {
	return
    }
    if {![winfo exists $client]} {
	return
    }
    if {$client != $data(client)} {
	return
    }
    if [info exists data(cancel)] {
	catch {
	    unset data(cancel)
	}
	return
    }

    if [tixBalloon::GrabBad $w $w] {
	return
    }

    tixBalloon::Activate $w
}

proc tixBalloon::ClientDestroy {w client} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    if {$data(client) == $client} {
	tixBalloon::Deactivate $w
	set data(client) {}
    }

    # Maybe thses have already been unset by the Destroy method
    #
    catch {unset data(m:$client)}
    catch {unset data(s:$client)}
}

#----------------------------------------------------------------------
# Popping up balloon:
#----------------------------------------------------------------------
proc tixBalloon::Activate {w} {
    upvar #0 $w data

    if [tixBalloon::GrabBad $w $w] {
	return
    }
    if {[winfo containing [winfo pointerx $w] [winfo pointery $w]] == {}} {
	return
    }

    switch $data(-state) {
	"both" {
	    tixBalloon::PopUp $w
	    tixBalloon::SetStatus $w
	}
	"balloon" {
	    tixBalloon::PopUp $w
	}
	"status" {
	    tixBalloon::SetStatus $w
	}
    }

    set data(isActive) 1

    after 200 tixBalloon::Verify $w
}


# %% Perhaps this is no more needed
#
proc tixBalloon::Verify {w} {
    upvar #0 $w data

    if {![winfo exists $w]} {
	return
    }
    if {!$data(isActive)} {
	return
    }

    if [tixBalloon::GrabBad $w $w] {
	tixBalloon::Deactivate $w
	return
    }
    if {[winfo containing [winfo pointerx $w] [winfo pointery $w]] == {}} {
	tixBalloon::Deactivate $w
	return
    }
    after 200 tixBalloon::Verify $w
}

proc tixBalloon::Deactivate {w} {
    upvar #0 $w data

    tixBalloon::PopDown $w
    tixBalloon::ClearStatus $w
    set data(isActive) 0
    catch {
	unset data(cancel)
    }
}

proc tixBalloon::PopUp {w} {
    upvar #0 $w data
    set client $data(client)
    if {"$data(m:$client)" == ""} {
	return
    }
    if [tixGetBoolean -nocomplain $data(-installcolormap)] {
	wm colormapwindows [winfo toplevel $data(client)] $w
    }

    # trick: the following lines allow the balloon window to
    # acquire a stable width and height when it is finally
    # put on the visible screen
    #
    set client $data(client)
    $data(w:message) config -text $data(m:$client)
    wm geometry $w +10000+10000
    wm deiconify $w
    raise $w
    update

    # The windows may become destroyed as a result of the "update" command
    #
    if {![winfo exists $w]} {
	return
    }
    if {![winfo exists $client]} {
	return
    }
    # Put it on the visible screen
    #
    set x [expr [winfo rootx $client]+[winfo width  $client]/2]
    set y [expr int([winfo rooty $client]+[winfo height $client]*0.1)]

    wm geometry $w +$x+$y
}

proc tixBalloon::PopDown {w} {
    upvar #0 $w data

    # Close the balloon
    #
    wm withdraw $w

    # We don't set the data(client) to be zero, so that the balloon
    # will re-appear only if you move out then in the client window
    # set data(client) {}
}

proc tixBalloon::SetStatus {w} {
    upvar #0 $w data

    if {![winfo exists $data(-statusbar)]} {
	return
    }
    if {![info exists data(s:$data(client))]} {
	return
    }

    set vv [$data(-statusbar) cget -textvariable]
    if {$vv == ""} {
	$data(-statusbar) config -text $data(s:$data(client))
    } else {
	uplevel #0 set $vv [list $data(s:$data(client))]
    }
}

proc tixBalloon::ClearStatus {w} {
    upvar #0 $w data

    if {![winfo exists $data(-statusbar)]} {
	return
    }

    # Clear the StatusBar widget
    #
    set vv [$data(-statusbar) cget -textvariable]
    if {$vv == ""} {
	$data(-statusbar) config -text ""
    } else {
	uplevel #0 set $vv [list ""]
    }
}

#----------------------------------------------------------------------
# PublicMethods:
#----------------------------------------------------------------------

# %% if balloon is already popped-up for this client, change mesage
#
proc tixBalloon::bind {w client args} {
    upvar #0 $w data

    if [info exists data(m:$client)] {
	set alreadyBound 1
    } else {
	set alreadyBound 0
    }

    set opt(-balloonmsg) {}
    set opt(-statusmsg)  {}
    set opt(-msg)        {}

    tixHandleOptions opt {-balloonmsg -msg -statusmsg} $args

    if {$opt(-balloonmsg) != {}} {
	set data(m:$client) $opt(-balloonmsg)
    } else {
	set data(m:$client) $opt(-msg)
    }
    if {$opt(-statusmsg) != {}} {
	set data(s:$client) $opt(-statusmsg)
    } else {
	set data(s:$client) $opt(-msg)
    }

    tixAppendBindTag $client TixBal$w
}

proc tixBalloon::post {w client} {
    upvar #0 $w data

    if {![info exists data(m:$client)]} {
	return
    }
    tixBalloon::Enter $w $client
    incr data(fakeEnter)
}

proc tixBalloon::unbind {w client} {
    upvar #0 $w data

    if [info exists data(m:$client)] {
	catch {unset data(m:$client)}
	catch {unset data(s:$client)}

	if [winfo exists $client] {
	    catch {tixDeleteBindTag $client TixBal$w}
	}
    }
}

#----------------------------------------------------------------------
#
# Utility function
#
#----------------------------------------------------------------------
#
# $w can be a widget name or a classs name
proc tixBalIgnoreWhenGrabbed {wc} {
    global tixBalloon
    set tixBalloon(g_ignore,$wc) {}
}

tixBalIgnoreWhenGrabbed TixComboBox
tixBalIgnoreWhenGrabbed Menu
tixBalIgnoreWhenGrabbed Menubutton
