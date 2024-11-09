# SHList.tcl --
#
#	This file implements Scrolled HList widgets
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

tixWidgetClass tixScrolledHList {
    -classname TixScrolledHList
    -superclass tixScrolledWidget
    -method {
    }
    -flag {
    }
    -configspec {
    }
    -default {
	{.scrollbar			auto}
	{*borderWidth			1}
	{*hlist.background		#c3c3c3}
	{*hlist.highlightBackground	#d9d9d9}
	{*hlist.relief			sunken}
	{*hlist.takeFocus		1}
	{*Scrollbar.background		#d9d9d9}
	{*Scrollbar.troughColor		#c3c3c3}
	{*Scrollbar.takeFocus		0}
	{*Scrollbar.relief		sunken}
	{*Scrollbar.width		15}
    }
}

proc tixScrolledHList::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget

    set data(pw:f1) \
	[frame $w.f1 -relief sunken -bd 2 -takefocus 0]
    set data(w:hlist) \
	[tixHList $w.f1.hlist -bd 0 -takefocus 1 -highlightthickness 0]

    pack $data(w:hlist) -in $data(pw:f1) -expand yes -fill both -padx 0 -pady 0

    set data(w:hsb) \
	[scrollbar $w.hsb -orient horizontal -takefocus 0]
    set data(w:vsb) \
	[scrollbar $w.vsb -orient vertical -takefocus 0]
    
    $data(pw:f1) config -highlightthickness \
	[$data(w:hsb) cget -highlightthickness]

    set data(pw:client) $data(pw:f1)
}

proc tixScrolledHList::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings

    $data(w:hlist) config \
	-xscrollcommand "$data(w:hsb) set"\
	-yscrollcommand "$data(w:vsb) set"\
	-sizecmd "tixScrolledWidget::Configure $w"

    $data(w:hsb) config -command "$data(w:hlist) xview"
    $data(w:vsb) config -command "$data(w:hlist) yview"

}

#----------------------------------------------------------------------
#
#		option configs
#----------------------------------------------------------------------
proc tixScrolledHList::config-takefocus {w value} {
    upvar #0 $w data
  
    $data(w:hlist) config -takefocus $value
}	

#----------------------------------------------------------------------
#
#		Widget commands
#----------------------------------------------------------------------


#----------------------------------------------------------------------
#
#		Private Methods
#----------------------------------------------------------------------

#----------------------------------------------------------------------
# virtual functions to query the client window's scroll requirement
#----------------------------------------------------------------------
proc tixScrolledHList::GeometryInfo {w mW mH} {
    upvar #0 $w data

    return [$data(w:hlist) geometryinfo $mW $mH]
}
