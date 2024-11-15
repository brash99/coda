# icon.tcl --
#
# This demonstration script creates a toplevel window containing
# buttons that display bitmaps instead of text.
#
# @(#) icon.tcl 1.1 95/05/26 15:56:32

set w .icon
catch {destroy $w}
toplevel $w
wm title $w "Iconic Button Demonstration"
wm iconname $w "icon"
positionWindow $w

label $w.msg -font $font -wraplength 5i -justify left -text "This window shows three ways of using bitmaps or images in radiobuttons and checkbuttons.  On the left are two radiobuttons, each of which displays a bitmap and an indicator.  In the middle is a checkbutton that displays a different image depending on whether it is selected or not.  On the right is a checkbutton that displays a single bitmap but changes its background color to indicate whether or not it is selected."
pack $w.msg -side top

frame $w.buttons
pack  $w.buttons -side bottom -expand y -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

image create bitmap flagup -file $tk_library/demos/images/flagup \
	-maskfile $tk_library/demos/images/flagup
image create bitmap flagdown -file $tk_library/demos/images/flagdown \
	-maskfile $tk_library/demos/images/flagdown
frame $w.frame -borderwidth 10
pack $w.frame -side top

checkbutton $w.frame.b1 -image flagdown -selectimage flagup \
	-indicatoron 0
$w.frame.b1 configure -selectcolor [$w.frame.b1 cget -background]
checkbutton $w.frame.b2 -bitmap @$tk_library/demos/images/letters \
	-indicatoron 0 -selectcolor SeaGreen1
frame $w.frame.left
pack $w.frame.left $w.frame.b1 $w.frame.b2 -side left -expand yes -padx 5m

radiobutton $w.frame.left.b3 -bitmap @$tk_library/demos/images/letters \
	-variable letters -value full
radiobutton $w.frame.left.b4 -bitmap @$tk_library/demos/images/noletters \
	-variable letters -value empty
pack $w.frame.left.b3 $w.frame.left.b4 -side top -expand yes
