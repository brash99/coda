#!/home3/ioi/bin/tix -f

set icons {}
set index 0
set llen [llength $argv]

proc LoadIcons {w} {
    global data argv icons index llen

    set n 0
    while {$index < $llen}  {
	set file [lindex $argv $index]

	set img [image create pixmap -file $file]
	set data($file,i) [$w create image 0 0 -image $img -anchor nw]
	set data($file,t) [$w create text  0 0 -text $file -anchor nw \
	    -font -*-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*]
	lappend icons $file

	$w coords $data($file,i) 10000 10000
	$w coords $data($file,t) 10000 10000

	update

	incr n
	incr index
	if {$n > 10} {
	    LayoutIcons $w
	    after 100 LoadIcons $w
	    return
	}
    }
}

proc TextSize {which w id} {
    set bbox [$w bbox $id]

    if {$which == "-width"} {
	return [expr [lindex $bbox 2] - [lindex $bbox 0] + 1]
    } else {
	return [expr [lindex $bbox 3] - [lindex $bbox 1] + 1]
    }
}

proc AlignLine {w maxH padx pady ipady line} {
    global data
    global icons

    # Re-align the items in the previous line
    #
    #
    foreach i $line {
	set image [$w itemcget $data($i,i) -image]
	set imageH [image height $image]
	set textH  [TextSize -height $w $data($i,t)]

	set itemH [expr $imageH + $textH + 2*$pady + $ipady]
	if {$itemH < $maxH} {
	    $w move $data($i,i) 0 [expr ($maxH - $itemH)]
	    $w move $data($i,t) 0 [expr ($maxH - $itemH)]
	}
    }
}

proc LayoutIcons {w} {
    global data
    global icons

    set W [winfo width $w]
    set y 0
    set x 0
    set maxH 0
    set line {}

    set padx 6
    set pady 6
    set ipady 2
    set len [llength $icons]
    set n 0
    set i 0
    while {$i < $len} {
	set file [lindex $icons $i]

	set image [$w itemcget $data($file,i) -image]
	set imageW [image width  $image]
	set imageH [image height $image]
	set textW  [TextSize -width  $w $data($file,t)]
	set textH  [TextSize -height $w $data($file,t)]

	if {$textW > $imageW} {
	    set xIncr $textW 
	} else {
	    set xIncr $imageW
	}
	incr xIncr [expr 2*$padx]
	if {([expr $x + $xIncr] > $W) && $n > 0} {

	    AlignLine $w $maxH $padx $pady $ipady $line
	    # Swap to next line
	    #
	    incr y $maxH
	    set x 0
	    set maxH 0
	    set n 0
	    set line {}
	    continue
	}    

	$w coords $data($file,i) $x $y
	$w coords $data($file,t) $x [expr $y + $imageH + $ipady]

	if {$textW > $imageW} {
	    $w move $data($file,i) [expr ($textW-$imageW)/2] 0
	} else {
	    $w move $data($file,t) [expr ($imageW-$textW)/2] 0
	}
	$w move $data($file,i) $padx $pady 
	$w move $data($file,t) $padx $pady

	incr x $xIncr

	set itemH [expr $imageH + $textH + 2*$pady + $ipady]
	if {$itemH > $maxH} {
	    set maxH $itemH
	}

	lappend line $file
	incr n
	incr i
    }

    if {$line != {}} {
	AlignLine $w $maxH $padx $pady $ipady $line
	incr y $maxH
    }

    $w config -height $y
    update
}

wm geometry . 600x400+100+100
tixScrolledWindow .s -shrink x
set f [.s subwidget window]
canvas $f.c
LoadIcons $f.c

pack .s -expand yes -fill both
pack $f.c -expand yes -fill both
bind $f.c <Configure> "tixWidgetDoWhenIdle LayoutIcons $f.c"
