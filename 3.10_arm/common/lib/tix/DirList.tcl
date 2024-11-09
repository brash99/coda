# DirList.tcl --
#
#	Implements the tixDirList widget.
#
# 	   - overrides the -browsecmd and -command options of the
#	     HList subwidget
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

tixWidgetClass tixDirList {
    -classname TixDirList
    -superclass tixScrolledHList
    -method {
	chdir
    }
    -flag {
	 -browsecmd -command -dircmd -disablecallback 
	 -root -rootname -showhidden -value
    }
    -configspec {
	{-browsecmd browseCmd BrowseCmd {}}
	{-command command Command {}}
	{-dircmd dirCmd DirCmd {}}
	{-disablecallback disableCallback DisableCallback false tixVerifyBoolean}
	{-root root Root /}
	{-rootname rootName RootName {}}
	{-showhidden showHidden ShowHidden false}
	{-value value Value {}}
    }
    -default {
	{.scrollbar			auto}
	{*borderWidth			1}
	{*hlist.background		#c3c3c3}
	{*hlist.indent			7}
	{*hlist.relief			sunken}
	{*hlist.height			10}
	{*hlist.width			20}
	{*hlist.padX			2}
	{*hlist.padY			0}
	{*hlist.wideSelection		0}
	{*hlist.drawBranch		0}
	{*hlist.highlightBackground	#d9d9d9}
	{*hlist.itemType		imagetext}
	{*hlist.takeFocus		1}
	{*Scrollbar.relief		sunken}
	{*Scrollbar.width		15}
	{*Scrollbar.background		#d9d9d9}
	{*Scrollbar.troughColor		#c3c3c3}
	{*Scrollbar.takeFocus		0}
    }
}

proc tixDirList::InitWidgetRec {w} {
    upvar #0 $w data

    tixChainMethod $w InitWidgetRec

    set data(oldDir) {{}}
}

proc tixDirList::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget
    tixDoWhenMapped $w "tixDirList::LoadDir $w"

    $data(w:hlist) config \
	-separator "/" \
	-selectmode "single"

    tixDirList::AddDir $w $data(-root)
}

proc tixDirList::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings

    $data(w:hlist) config \
	-browsecmd "tixDirList::Browse $w" \
	-command "tixDirList::Command $w"
}

proc tixDirList::AddDir {w dir} {
    upvar #0 $w data

    set path {}
    set prefix ""
    set first 0
    foreach name [split $dir /] {
	incr first
	if {$name == {}} {
	    if {$first == 1 && $data(-root) == "/"} {
		set path "/"
		set name "/"
	    } else {
		continue
	    }
	} else {
	    append path $prefix
	    append path $name
	    set prefix "/"
	}
	if {![$data(w:hlist) info exists $path]} {
	    $data(w:hlist) add $path -text $name \
		-image [tix getimage openfolder]
	}
	if {$path == $data(-root) && $data(-rootname) != {}} {
	    $data(w:hlist) entryconfig $path -text $data(-rootname) 
	}
    }
}

proc tixDirList::UserListDirs {w aDir} {
    upvar #0 $w data
    tixBusy $w on $data(w:hlist)

    set olddir {}
    foreach fname [lsort [tixEvalCmdBinding $w $data(-dircmd) {} \
	$data(-value) $data(-showhidden)]] {

	set fname [file tail $fname]
	if {$data(-value) == "/"} {
	    set dir /$fname
	} else {
	    set dir $data(-value)/$fname
	}

	if {$dir == $olddir} {
	    continue
	}

	if {![$data(w:hlist) info exists $dir]} {
	    $data(w:hlist) add $dir -text $fname \
		-image [tix getimage folder]
	}
	set olddir $dir
    }
    tixWidgetDoWhenIdle tixBusy $w off $data(w:hlist)
}

proc tixDirList::ListDirs {w aDir} {
    upvar #0 $w data

    if {$data(-dircmd) != {}} {
	tixDirList::UserListDirs $w $aDir
	return
    }

    set appPWD [pwd]

    if [catch {cd $data(-value)} err] {
	# The user has entered an invalid directory
	# %% todo: prompt error, go back to last succeed directory
	cd $appPWD
	return
    }

    $data(w:hlist) entryconfig $aDir \
	-image [tix getimage act_folder]

    if [tixGetBoolean -nocomplain $data(-showhidden)] {
	if [catch {set names [lsort [glob -nocomplain * .*]]} err] {
	    # Cannot read directory
	    # %% todo: show directory permission denied
	    cd $appPWD
	    return
	}
    } else {
	if [catch {set names [lsort [glob -nocomplain *]]} err] {
	    cd $appPWD
	    return
	}
    }
    tixBusy $w on $data(w:hlist)

    foreach fname $names {
	if {![string compare . $fname] || ![string compare .. $fname]} {
	    continue
	}
	if {$data(-value) == "/"} {
	    set dir /$fname
	} else {
	    set dir $data(-value)/$fname
	}
	if {[file isdirectory $dir] && ![$data(w:hlist) info exists $dir]} {
	    $data(w:hlist) add $dir -text $fname \
		-image [tix getimage folder]
	}
    }
    cd $appPWD
    tixWidgetDoWhenIdle tixBusy $w off $data(w:hlist)
}

proc tixDirList::LoadDir {w} {
    if {![winfo exists $w]} {
	return
    }
    if {![winfo ismapped [winfo toplevel $w]]} {
	tixDoWhenMapped [winfo toplevel $w] "tixDirList::LoadDir $w"
	return
    }

    upvar #0 $w data

    if {$data(-value) == {}} {
	global env
	if {[info exists env(PWD)]} {
	    set data(-value) $env(PWD)
	} else {
	    set data(-value) [pwd]
	}
    }

    if {$data(oldDir) == $data(-value)} {
	return
    }

    if [$data(w:hlist) info exists $data(-root)] {
	$data(w:hlist) delete offsprings $data(-root)
    } else {
	$data(w:hlist) delete all
    }

    tixDirList::AddDir $w $data(-value)

    $data(w:hlist) entryconfig $data(-root) \
	-image [tix getimage openfolder]

    tixDirList::ListDirs $w $data(-value)
    $data(w:hlist) anchor set $data(-value)
    $data(w:hlist) select clear
    $data(w:hlist) select set $data(-value)

    # Make sure it is visible
    # ToDo: also make sure child is visible!
    $data(w:hlist) see $data(-value)

    set $data(oldDir) $data(-value) 
}

proc tixDirList::ChangeDir {w value} {
    upvar #0 $w data

    set data(-value) $value
    tixDirList::LoadDir $w

    if {$data(-command) != {} && !$data(-disablecallback)} {
	set bind(specs) ""
	tixEvalCmdBinding $w $data(-command) bind $data(-value)
    }
}

proc tixDirList::Command {w args} {
    upvar #0 $w data

    set value [tixEvent flag V]
    tixDirList::ChangeDir $w $value
}

proc tixDirList::Browse {w args} {
    upvar #0 $w data

    set value [tixEvent flag V]

    if {$data(-value) != {} && $data(-value) != $value} {
	if {[$data(w:hlist) info children $data(-value)] == {}} {
	    $data(w:hlist) entryconfig  $data(-value)\
		-image [tix getimage folder]
	} else {
	    $data(w:hlist) entryconfig  $data(-value)\
		-image [tix getimage openfolder]
	}

	set data(-value) $value

	$data(w:hlist) entryconfig  $data(-value)\
	    -image [tix getimage act_folder]
    }


    if {$data(-browsecmd) != {}} {
	set bind(specs) ""
	tixEvalCmdBinding $w $data(-browsecmd) bind $data(-value)
    }
}

#----------------------------------------------------------------------
# Config options
#----------------------------------------------------------------------
proc tixDirList::config-value {w value} {
    upvar #0 $w data

    if {[$data(w:hlist) info exists $value]} {
	$data(w:hlist) anchor set $value
	$data(w:hlist) select clear
	$data(w:hlist) select set $value
	$data(w:hlist) see $value
	set data(-value) $value
	if {$data(-command) != {} && !$data(-disablecallback)} {
	    set bind(specs) ""
	    tixEvalCmdBinding $w $data(-command) bind $data(-value)
	}
	return
    }

    tixWidgetDoWhenIdle tixDirList::ChangeDir $w $value
}

proc tixDirList::config-root {w value} {
    upvar #0 $w data

    $data(w:hlist) delete all
}

proc tixDirList::config-showhidden {w value} {
    upvar #0 $w data

    tixWidgetDoWhenIdle tixDirList::LoadDir $w
}

#----------------------------------------------------------------------
# Public methods
#----------------------------------------------------------------------
proc tixDirList::chdir {w value} {
    upvar #0 $w data

    tixDirList::ChangeDir $w [tixFile trimslash [tixFile tildesubst $value]]
}
