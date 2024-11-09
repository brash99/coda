tixWidgetClass tixPopupMenu {
    -classname TixPopupMenu
    -superclass tixShell
    -method {
	bind post unbind
    }
    -flag {
	-installcolormap -state -title 
    }
    -configspec {
	{-installcolormap installColormap InstallColormap false}
	{-state state State normal}
	{-cursor corsor Corsur arrow}
    }
    -default  {
	{*Menu.tearOff			0}
    }
}

proc tixPopupMenu::InitWidgetRec {w} {
    upvar #0 $w data

    tixChainMethod $w InitWidgetRec

    set data(g:clients)   {}
}

proc tixPopupMenu::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget

    wm overrideredirect $w 1
    wm withdraw $w

    set data(w:menubutton) [menubutton $w.menubutton -text $data(-title) \
			    -menu $w.menubutton.menu -anchor w]
    set data(w:menu) [menu $w.menubutton.menu]

    pack $data(w:menubutton) -expand yes -fill both
}

proc tixPopupMenu::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings

    bind $data(w:menubutton) <3>      "+ tixPopupMenu::Unpost $w %X %Y"
    bind $data(w:menu)       <Unmap>  "+ tixPopupMenu::Unmap $w"

    bind TixPopupMenu:$w <3> "tixPopupMenu::post $w %W %x %y"
}

#----------------------------------------------------------------------
# PrivateMethods:
#----------------------------------------------------------------------
proc tixPopupMenu::Unpost {w rootx rooty} {
    upvar #0 $w data
    global tkPriv

    tkMbButtonUp $data(w:menubutton)
}

proc tixPopupMenu::Unmap {w} {
    upvar #0 $w data
    wm withdraw $w
}

proc tixPopupMenu::Destructor {w} {
    upvar #0 $w data

    foreach client $data(g:clients) {
	if [winfo exists $client] {
	    tixDeleteBindTag $client TixPopupMenu:$w
	}
    }

    tixChainMethod $w Destructor
}

proc tixPopupMenu::config-title {w value} {
    upvar #0 $w data

    $data(w:menubutton) config -text $value
}

#----------------------------------------------------------------------
# PublicMethods:
#----------------------------------------------------------------------
proc tixPopupMenu::bind {w args} {
    upvar #0 $w data

    foreach client $args {
	if {[lsearch $data(g:clients) $client] == -1} {
	    lappend data(g:clients) $client
	    tixAppendBindTag $client TixPopupMenu:$w
	}
    }
}

proc tixPopupMenu::unbind {w args} {
    upvar #0 $w data

    foreach client $args {
	if [winfo exists $client] {
	    set index [lsearch $data(g:clients) $client]
	    if {$index != -1} {
		tixDeleteBindTag $client TixPopupMenu:$w
		set data(g:clients) [lreplace $data(g:clients) $index $index]
	    }
	}
    }
}

proc tixPopupMenu::post {w client x y} {
    upvar #0 $w data
    global tkPriv

    if {$data(-state)  == "disabled"} {
	return
    }

    if [tixGetBoolean -nocomplain $data(-installcolormap)] {
	wm colormapwindows . "$w"
    }


    set rootx [expr $x + [winfo rootx $client]]
    set rooty [expr $y + [winfo rooty $client]]

    set menuWidth [winfo reqwidth $data(w:menu)]
    set width     [winfo reqwidth  $w]
    set height    [winfo reqheight $w]

    if {$width < $menuWidth} {
	set width $menuWidth
    }

    set wx $rootx
    set wy $rooty

    # trick: the following lines allow the popup menu
    # acquire a stable width and height when it is finally
    # put on the visible screen. Advoid flashing
    #
    wm geometry $w +10000+10000
    wm deiconify $w
    raise $w

    update
    wm geometry $w $width\x$height+$wx+$wy
    update

    tkMbEnter $data(w:menubutton)
    tkMbPost $tkPriv(inMenubutton) $rootx $rooty
}
