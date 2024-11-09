# LabEntry.tcl --
#
# 	TixLabelEntry Widget: an entry box with a label
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


tixWidgetClass tixLabelEntry {
    -classname TixLabelEntry
    -superclass tixLabelWidget
    -method {
    }
    -flag {
	-disabledforeground -state
    }
    -forcecall {
	-state
    }
    -static {
    }
    -configspec {
	{-disabledforeground disabledForeground DisabledForeground #303030}
	{-state state State normal}
    }
    -default {
	{.borderWidth 			0}
	{*entry.relief			sunken}
	{*entry.width			7}
	{*label.anchor			e}
	{*label.borderWidth		0}
	{*Label.font                   -Adobe-Helvetica-Bold-R-Normal--*-120-*}
	{*Entry.background		#c3c3c3}
    }
}

proc tixLabelEntry::ConstructFramedWidget {w frame} {
    upvar #0 $w data

    tixChainMethod $w ConstructFramedWidget $frame

    set data(w:entry)  [entry $frame.entry]
    pack $data(w:entry) -side left -expand yes -fill both

    # This value is used to configure the disable/normal fg of the ebtry
    #
    set data(entryfg) [lindex [$data(w:entry) config -fg] 4]
    set data(labelfg) [lindex [$data(w:label) config -fg] 4]
}

#----------------------------------------------------------------------
#                           CONFIG OPTIONS
#----------------------------------------------------------------------
proc tixLabelEntry::config-state {w value} {
    upvar #0 $w data

    if {$value == "normal"} {
	$data(w:label) config -fg $data(labelfg)
	$data(w:entry) config -state $value -fg $data(entryfg)
    } else {
	$data(w:label) config -fg $data(-disabledforeground)
	$data(w:entry) config -state $value -fg $data(-disabledforeground)
    }
}
