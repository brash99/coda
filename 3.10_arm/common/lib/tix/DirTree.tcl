# DirTree.tcl --
#
#	Implements directory tree for Unix file systems
#
#       What the indicators mean:
#
#	(+): There are some subdirectories in this directory which are not
#	     currently visible.
#	(-): This directory has some subdirectories and they are all visible
#
#      none: The dir has no subdirectori(es).
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

tixWidgetClass tixDirTree {
    -classname TixDirTree
    -superclass tixVTree
    -method {
	activate chdir refresh
    }
    -flag {
	-browsecmd -command -directory -disablecallback -showhidden -value
    }
    -configspec {
	{-browsecmd browseCmd BrowseCmd {}}
	{-command command Command {}}
	{-directory directory Directory {}}
	{-disablecallback disableCallback DisableCallback 0 tixVerifyBoolean}
	{-showhidden showHidden ShowHidden 0 tixVerifyBoolean}
    }
    -alias {
	{-value	-directory}
    }
    -default {
	{.scrollbar			auto}
	{*Scrollbar.background          #d9d9d9}
	{*Scrollbar.relief              sunken}
	{*Scrollbar.takeFocus           0}
	{*Scrollbar.troughColor         #c3c3c3}
	{*Scrollbar.width               15}
	{*borderWidth                   1}
	{*hlist.indicator               1}
	{*hlist.background              #c3c3c3}
	{*hlist.drawBranch              1}
	{*hlist.height                  10}
	{*hlist.highlightBackground      #d9d9d9}
	{*hlist.indent                  20}
	{*hlist.itemType                imagetext}
	{*hlist.padX                    3}
	{*hlist.padY                    0}
	{*hlist.relief                  sunken}
	{*hlist.takeFocus               1}
	{*hlist.wideSelection           0}
	{*hlist.width                   20}
    }
}

proc tixDirTree::InitWidgetRec {w} {
    upvar #0 $w data

    tixChainMethod $w InitWidgetRec

    if {$data(-directory) == {}} {
	global env
	if {[info exists env(PWD)]} {
	    set data(-directory) $env(PWD)
	} else {
	    set data(-directory) [pwd]
	}
    }
}

proc tixDirTree::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget
    tixDoWhenMapped $w "tixDirTree::StartUp $w"

    $data(w:hlist) config \
	-separator "/" \
	-selectmode "single" -drawbranch 1
}

proc tixDirTree::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings

# %% do I still need this?
#   bind $data(w:hlist) <3> "tixDirTree::DeleteSib $w %x %y"
}

proc tixDirTree::DeleteSib {w x y} {
    upvar #0 $w data

    set ent [$data(w:hlist) nearest $y]

    if {$ent != ""} {
	$data(w:hlist) anchor set $ent

	for {set e $ent} {$e != "/"} {set e [$data(w:hlist) info parent $e]} {
	    $data(w:hlist) delete siblings $e
	}
	tixDirTree::Browse $w $ent
    }
}

# %% This functions needs to be optimized
#
#
proc tixDirTree::HasSubDir {w dir} {
    upvar #0 $w data

    if $data(-showhidden) {
	set pattern "$dir/* $dir/.*"
    } else {
	set pattern $dir/*
    }

    if [catch {set names [eval glob -nocomplain $pattern]}] {
	# Cannot read directory
	# %% todo: show directory permission denied
	return 0
    }

    # Get rid of . and ..
    #
    foreach f $names {
	if ![string compare [file tail $f] "."] {
	    continue
	}
	if ![string compare [file tail $f] ".."] {
	    continue
	}
	if [file isdirectory $f] {
	    return 1
	}
    }
    return 0
}

proc tixDirTree::AddToList {w dir name image} {
    upvar #0 $w data

    $data(w:hlist) add $dir -text $name -image $image

    if [tixDirTree::HasSubDir $w $dir] {
	tixVTree::SetMode $w $dir open
    }
}

# Add $dir and all ancestors of $dir into the HList widget
#
#
proc tixDirTree::AddDir {w dir} {
    upvar #0 $w data

    if [$data(w:hlist) info exists $dir] {
	return
    }

    if {$dir != "/"} {
	set parentDir [file dirname $dir]

	# This is a bug fix for Tcl 7.5 a2
	#
	if {![string compare $parentDir ":"]} {
	    set parentDir /
	}
	if {![string compare $parentDir "."]} {
	    set parentDir /
	}

	if {![$data(w:hlist) info exists $parentDir]} {
	    tixDirTree::AddDir $w $parentDir
	}
	tixDirTree::AddToList $w $dir [file tail $dir] \
	    [tix getimage folder]
    } else {
	$data(w:hlist) add $dir -text / \
	    -image [tix getimage folder]
	tixVTree::SetMode $w $dir open
    }
}


# Add all the sub directories of $dir into the HList widget
#
#
proc tixDirTree::ListDirs {w dir} {
    upvar #0 $w data

    set appPWD [pwd]

    if [catch {cd $dir} err] {
	# The user has entered an invalid directory
	# %% todo: prompt error, go back to last succeed directory
	cd $appPWD
	return
    }

    if $data(-showhidden) {
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

    foreach fname $names {
	if {![string compare . $fname] || ![string compare .. $fname]} {
	    continue
	}
	if {$dir == "/"} {
	    set subdir /$fname
	} else {
	    set subdir $dir/$fname
	}
	if [file isdirectory $subdir] {
	    if {![$data(w:hlist) info exists $subdir]} {
		tixDirTree::AddToList $w $subdir $fname \
		    [tix getimage folder]
	    }
	}
    }

    cd $appPWD
}

proc tixDirTree::LoadDir {w dir {mode toggle}} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    # Add the directory and set it to the active directory
    #
    set data(oldDir) $dir
    tixDirTree::AddDir $w $dir
    $data(w:hlist) entryconfig $dir -image [tix getimage act_folder]

    if {$mode == "toggle"} {
	if {[$data(w:hlist) info children $dir] == {}} {
	    set mode expand
	} else {
	    set mode flatten
	}
    }

    if {$mode == "expand"} {
	tixDirTree::ListDirs $w $dir
	if {[$data(w:hlist) info children $dir] == {}} {
	    tixVTree::SetMode $w $dir none
	} else {
	    tixVTree::SetMode $w $dir close
	}
    } else {
	$data(w:hlist) delete offsprings $dir
	tixVTree::SetMode $w $dir open
    }
}

proc tixDirTree::ToggleDir {w value mode} {
    upvar #0 $w data

    tixDirTree::LoadDir $w $value $mode
    tixDirTree::CallCommand $w
}

proc tixDirTree::CallCommand {w} {
    upvar #0 $w data

    if {$data(-command) != {} && !$data(-disablecallback)} {
	set bind(specs) {%V}
	set bind(%V)    $data(-directory)

	tixEvalCmdBinding $w $data(-command) bind $data(-directory)
    }
}

proc tixDirTree::CallBrowseCmd {w ent} {
    upvar #0 $w data

    if {$data(-browsecmd) != {} && !$data(-disablecallback)} {
	set bind(specs) {%V}
	set bind(%V)    $data(-directory)

	tixEvalCmdBinding $w $data(-browsecmd) bind $data(-directory)
    }
}

proc tixDirTree::StartUp {w} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    tixDirTree::LoadDir $w $data(-directory)
}

proc tixDirTree::ChangeDir {w value {forced 0}} {
    upvar #0 $w data

    if {!$forced && $data(-directory) == $value} {
	return
    }

    if {!$forced && [$data(w:hlist) info exists $value]} {
	# Set the old directory to "non active"
	#
	if [$data(w:hlist) info exists $data(-directory)] {
	    $data(w:hlist) entryconfig $data(-directory) \
		-image [tix getimage folder]
	}

	$data(w:hlist) entryconfig $value  \
		-image [tix getimage act_folder]

    } else {
	if {$forced} {
	    if {[$data(w:hlist) info children $value] == {}} {
		set mode flatten
	    } else {
		set mode expand
	    }
	} else {
	    set mode toggle
	}
	tixDirTree::LoadDir $w $value $mode
	tixDirTree::CallCommand $w
    }
    set data(-directory) $value
}


#----------------------------------------------------------------------
#
# Virtual Methods
#
#----------------------------------------------------------------------
proc tixDirTree::OpenCmd {w ent} {
    tixDirTree::ToggleDir $w $ent expand
    tixDirTree::ChangeDir $w $ent
    tixDirTree::CallBrowseCmd $w $ent
}

proc tixDirTree::CloseCmd {w ent} {
    tixDirTree::ToggleDir $w $ent flatten
    tixDirTree::ChangeDir $w $ent
    tixDirTree::CallBrowseCmd $w $ent
}

proc tixDirTree::Command {w B} {
    upvar #0 $w data
    upvar $B bind

    set ent [tixEvent flag V]
    tixChainMethod $w Command $B

    if {$data(-command) != {}} {
	tixEvalCmdBinding $w $data(-command) bind $ent
    }
}

# This is a virtual method
#
proc tixDirTree::BrowseCmd {w B} {
    upvar #0 $w data
    upvar $B bind

    set ent [tixEvent flag V]

#    if {[$data(w:hlist) indicator exist $ent] && 
#	[$data(w:hlist) info children $ent] == {}} {
#	
#	tixVTree::Activate $w $ent open
#   }

    tixDirTree::ChangeDir $w $ent

    if {$data(-browsecmd) != {}} {
	tixEvalCmdBinding $w $data(-browsecmd) bind $ent
    }
}

#----------------------------------------------------------------------
#
# Public Methods
#
#----------------------------------------------------------------------
proc tixDirTree::chdir {w value} {
    tixDirTree::ChangeDir $w $value
}

proc tixDirTree::refresh {w {dir {}}} {
    upvar #0 $w data

    if {$dir == {}} {
	set dir $data(-directory)
    }

    tixDirTree::ChangeDir $w $dir 1

    foreach sub [$data(w:hlist) info children $dir] {
	if {![file exists $sub]} {
	    $data(w:hlist) delete entry $sub
	}
    }
}

proc tixDirTree::config-directory {w value} {
    tixDirTree::ChangeDir $w $value
}
