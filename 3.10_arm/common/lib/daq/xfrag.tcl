#!/bin/sh
#\
    exec $CODA_BIN/dpwish -f "$0" ${1+"$@"}
#


proc draw_menu {} {
    global env
    
    set images_library $env(CODA)/common/images
    
    set coda_logo [image create photo -file $images_library/gif/RCLogo.gif]
    
    set w [frame .f1 -bd 2 -relief raised]
    
    label $w.logo -bg grey -padx 0 -pady 0 -image $coda_logo 
    
    menubutton $w.file -menu $w.file.m -text File -underline 0 \
	-takefocus 0
    
    menu $w.file.m
    $w.file.m add command -label "Exit     " -command exit -underline 1 \
	-accelerator "Ctrl+X"
    
    # Accelerator bindings
    
    bind all <Control-x> "exit"
    pack $w.file -in $w -side left
    pack $w.logo -side left  -padx 6 -expand yes
    pack $w -side top -fill x -expand yes
    return $w
}

proc draw_title {} {
    set w [frame .title -relief raised -borderwidth 2]
    label $w.a -width 10 -relief ridge -borderwidth 2 \
	-text "Component"
    label $w.t -width 10 -relief ridge -borderwidth 2 \
	-text "stream"
    label $w.f -width 10 -relief ridge -borderwidth 2 \
	-text "size"

    label $w.q1 -width 4 -relief ridge -borderwidth 2 \
	-text "get "

    label $w.q2 -width 4 -relief ridge -borderwidth 2 \
	-text "put "

    label $w.q3 -width 4 -relief ridge -borderwidth 2 \
	-text "del "

    label $w.q4 -width 4 -relief ridge -borderwidth 2 \
	-text "tot "

    label $w.q5 -width 4 -relief ridge -borderwidth 2 \
	-text "full"

    pack $w.a $w.t $w.f $w.q1 $w.q2 $w.q3 $w.q4 $w.q5 -in $w -side left
    pack $w -side top
    return $w
}

proc draw_widget {item} {
    set w [frame .frame$item]
    label $w.a -width 10 -relief ridge -borderwidth 2
    label $w.t -width 10 -relief ridge -borderwidth 2
    label $w.f -width 10 -relief ridge -borderwidth 2

    label $w.q1 -width 4 -relief ridge -borderwidth 2
    label $w.q2 -width 4 -relief ridge -borderwidth 2
    label $w.q3 -width 4 -relief ridge -borderwidth 2
    label $w.q4 -width 4 -relief ridge -borderwidth 2
    label $w.q5 -width 4 -relief ridge -borderwidth 2
    
    pack $w.a $w.t $w.f $w.q1 $w.q2 $w.q3 $w.q4 $w.q5 -in $w -side left
    pack $w -side top
    return $w
}


proc update_rows {name time} {
    if {![catch "DP_ask $name frag_sized" res]} {
	set ix 0
	foreach item $res {
	    .frame$ix.a config -text [lindex $item 0]
	    .frame$ix.t config -text [lindex $item 1]
	    .frame$ix.f config -text [lindex $item 2]
	    if {![catch "DP_ask $name [lindex $item 0] fifo_stats" res2]} {
	    .frame$ix.q1 config -text [lindex $res2 1]
	    .frame$ix.q2 config -text [lindex $res2 2]
	    .frame$ix.q3 config -text [lindex $res2 3]
	    .frame$ix.q4 config -text [lindex $res2 4]
	    .frame$ix.q5 config -text [lindex $res2 5]
		
	    }
	    incr ix
	}
    }

    dp_after [expr [set time] * 1000] update_rows $name $time
}

draw_menu
draw_title

if { $argc != 3 } {
 
    set name [lindex $argv 0]
    set time [lindex $argv 1]
    for {set ix 0} {$ix < 32} {incr ix} {
	set w [draw_widget $ix]
    }
    update_rows $name $time
} else {
    puts "usage : $argv0 ?EB name? ?rep rate? "
    exit
}
