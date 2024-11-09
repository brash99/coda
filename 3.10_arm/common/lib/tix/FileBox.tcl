# FileBox.tcl --
#
#	Implements the File Selection Box widget.
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


# ToDo
#   (1)	If user has entered an invalid directory, give an error dialog
#

tixWidgetClass tixFileSelectBox {
    -superclass tixPrimitive
    -classname  TixFileSelectBox
    -method {
	filter invoke
    }
    -flag {
	-browsecmd -command -dir -directory -disablecallback
	-grab -pattern -selection -value
    }
    -configspec {
	{-browsecmd browseCmd BrowseCmd {}}
	{-command command Command {}}
	{-directory directory Directory {}}
	{-disablecallback disableCallback DisableCallback false tixVerifyBoolean}
	{-grab grab Grab global}
	{-pattern pattern Pattern *}
	{-value value Value {}}
    }
    -alias {
	{-selection -value}
	{-dir -directory}
    }
    -forcecall {
	-value
    }
    -default {
	{.relief			raised}
	{*filelist*Listbox.takeFocus	true}
	{.borderWidth 			1}
	{*Label.anchor			w}
	{*Label.borderWidth		0}
	{*Label.font                   -Adobe-Helvetica-Bold-R-Normal--*-120-*}
	{*TixComboBox*scrollbar		auto}
	{*TixComboBox*Label.anchor	w}
	{*TixScrolledListBox.scrollbar	auto}
	{*Listbox.exportSelection	false}
	{*Listbox.exportSelection	false}
    }
}

option add *TixFileSelectBox*directory*Label.text  "Directories:"
option add *TixFileSelectBox*directory*Label.underline 0
option add *TixFileSelectBox*file*Label.text  "Files:"
option add *TixFileSelectBox*file*Label.underline 2

option add *TixFileSelectBox*filter.label "Filter:"
option add *TixFileSelectBox*filter*label.underline 3
option add *TixFileSelectBox*filter.labelSide top

option add *TixFileSelectBox*selection.label "Selection:"
option add *TixFileSelectBox*selection*label.underline 0
option add *TixFileSelectBox*selection.labelSide top

proc tixFileSelectBox::InitWidgetRec {w} {
    upvar #0 $w data
    global env

    tixChainMethod $w InitWidgetRec

    if {$data(-directory) == {}} {
	global env

	if {[info exists env(PWD)]} {
	    set data(-directory) $env(PWD)
	} else {
	    set data(-directory) [pwd]
	}
    }

    set data(flag)      0
    set data(fakeDir)   0
}

#----------------------------------------------------------------------
#		Construct widget
#----------------------------------------------------------------------
proc tixFileSelectBox::ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget

    set frame1 [tixFileSelectBox::CreateFrame1 $w]
    set frame2 [tixFileSelectBox::CreateFrame2 $w]
    set frame3 [tixFileSelectBox::CreateFrame3 $w]

    pack $frame1 -in $w -side top -fill x
    pack $frame3 -in $w -side bottom -fill x
    pack $frame2 -in $w -side top -fill both -expand yes

    tixSetSilent $data(w:filter) \
	[tixFileSelectBox::GetFilter $w $data(-directory) $data(-pattern)]

    $data(w:filter) addhistory \
	[tixFileSelectBox::GetFilter $w $data(-directory) $data(-pattern)]
}

proc tixFileSelectBox::CreateFrame1 {w} {
    upvar #0 $w data

    frame $w.f1 -border 10
    tixComboBox $w.f1.filter -history true\
	-command "$w filter" -anchor e \
	-options {
	    slistbox.scrollbar auto
	    listbox.height 5
	    label.anchor w
	}
    set data(w:filter) $w.f1.filter

    pack $data(w:filter) -side top -expand yes -fill both
    return $w.f1
}

proc tixFileSelectBox::CreateFrame2 {w} {
    upvar #0 $w data

    tixPanedWindow $w.f2 -orientation horizontal
    #     THE LEFT FRAME
    #-----------------------
    set dir [$w.f2 add directory -size 120]
    $dir config -relief flat
    label $dir.lab
    set data(w:dirlist) [tixScrolledListBox $dir.dirlist\
		       -scrollbar auto\
		       -options {listbox.width 4 listbox.height 6}]

    pack $dir.lab -side top -fill x -padx 10
    pack $data(w:dirlist) -side bottom -expand yes -fill both -padx 10

    #     THE RIGHT FRAME
    #-----------------------
    set file [$w.f2 add file -size 160]
    $file config -relief flat
    label $file.lab
    set data(w:filelist) [tixScrolledListBox $file.filelist \
		       -scrollbar auto\
		       -options {listbox.width 4 listbox.height 6}]

    pack $file.lab -side top -fill x -padx 10
    pack $data(w:filelist) -side bottom -expand yes -fill both -padx 10

    return $w.f2
}

proc tixFileSelectBox::CreateFrame3 {w} {
    upvar #0 $w data

    frame $w.f3 -border 10
    tixComboBox $w.f3.selection -history true\
	-command "tixFileSelectBox::SelInvoke $w" \
	-anchor e \
	-options {
	    slistbox.scrollbar auto
	    listbox.height 5
	    label.anchor w
	}

    set data(w:selection) $w.f3.selection

    pack $data(w:selection) -side top -fill both

    return $w.f3
}

proc tixFileSelectBox::SelInvoke {w args} {
    upvar #0 $w data

    set event [tixEvent type]

    if {$event != "<FocusOut>" && $event != "<Tab>"} {
	$w invoke
    }
}


#----------------------------------------------------------------------
#                           BINDINGS
#----------------------------------------------------------------------

proc tixFileSelectBox::SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings

    tixDoWhenMapped $w "tixFileSelectBox::FirstMapped $w"

    $data(w:dirlist) config \
	-browsecmd "tixFileSelectBox::SelectDir $w" \
	-command   "tixFileSelectBox::InvokeDir $w"

    $data(w:filelist) config \
	-browsecmd "tixFileSelectBox::SelectFile $w" \
	-command   "tixFileSelectBox::InvokeFile $w"
}

#----------------------------------------------------------------------
#                           CONFIG OPTIONS
#----------------------------------------------------------------------
proc tixFileSelectBox::config-directory {w value} {
    upvar #0 $w data

    set value [tixFile tildesubst $value]
    set value [tixFile trimslash $value]

    tixSetSilent $data(w:filter) \
	[tixFileSelectBox::GetFilter $w $value $data(-pattern)]

    $w filter
    set data(-directory) $value
    return $value
}

proc tixFileSelectBox::config-pattern {w value} {
    upvar #0 $w data

    if {$value == {}} {
	set data(-pattern) "*"
    } else {
	set data(-pattern) $value
    }
    
    tixSetSilent $data(w:filter) \
	[tixFileSelectBox::GetFilter $w $data(-directory) $value]

    # Returning a value means we have overridden the value and updated
    # the widget record ourselves.
    #
    return $data(-pattern)
}

proc tixFileSelectBox::config-value {w value} {
    upvar #0 $w data

    tixSetSilent $data(w:selection) $value
}

#----------------------------------------------------------------------
#                    PUBLIC METHODS
#----------------------------------------------------------------------
proc tixFileSelectBox::filter {w args} {
    upvar #0 $w data

    $data(w:filter) popdown
    set filter [tixFileSelectBox::InterpFilter $w]
    tixFileSelectBox::LoadDir $w
}

# InterpFilter:
#	Interp the value of the w:filter widget. 
#
# Side effects:
#	Changes the fields data(-directory) and data(-pattenn) 
#
proc tixFileSelectBox::InterpFilter {w {filter {}}} {
    upvar #0 $w data

    if {$filter == {}} {
	set filter [$data(w:filter) cget -selection]
	if {$filter == {}} {
	    set filter [$data(w:filter) cget -value]
	}
    }

    set filter [tixFile tildesubst $filter]
    
    if [file isdir $filter] {
	set data(-directory) [tixFile trimslash $filter]
	set data(-pattern) "*"
    } else {
	set data(-directory)  [file dir $filter]
	set data(-pattern)    [file tail $filter]
    }

    set data(-directory) [tixResolveDir $data(-directory)]

    set filter [tixFileSelectBox::GetFilter $w $data(-directory) \
        $data(-pattern)]

    tixSetSilent $data(w:filter) $filter

    return $filter
}

proc tixFileSelectBox::invoke {w args} {
    upvar #0 $w data

    if {[$data(w:selection) cget -value] !=
	[$data(w:selection) cget -selection]} {
	    $data(w:selection) invoke
	    return
    }
    
    # record the filter
    #
    set filter [tixFileSelectBox::InterpFilter $w]
    $data(w:filter) addhistory $filter

    # record the selection
    #
    set value [$data(w:selection) cget -value]
    set value [tixFile tildesubst $value]
    set value [tixFile trimslash $value]

    if {[string index $value 0] != "/"} {
	set value $data(-directory)/$value
	set value [tixFile tildesubst $value]
	set value [tixFile trimslash $value]
	tixSetSilent $data(w:selection) $value
    }
    set data(-value) $value

    $data(w:selection) addhistory $data(-value)

    $data(w:filter) align
    $data(w:selection)  align

    if {$data(-command) != {} && !$data(-disablecallback)} {
	set bind(specs) "%V"
	set bind(%V) $data(-value)
	tixEvalCmdBinding $w $data(-command) bind $data(-value)
    }
}

#----------------------------------------------------------------------
#                    INTERNAL METHODS
#----------------------------------------------------------------------
proc tixFileSelectBox::GetFilter {w dir pattern} {
    if {$dir == "/"} {
	return /$pattern
    } else {
	return $dir/$pattern
    }
}

proc tixFileSelectBox::LoadDirIntoLists {w} {
    upvar #0 $w data

    $data(w:dirlist) subwidget listbox delete 0 end
    $data(w:filelist) subwidget listbox delete 0 end

    set appPWD [pwd]

    if [catch {cd $data(-directory)} err] {
	# The user has entered an invalid directory
	# %% todo: prompt error, go back to last succeed directory
	cd $appPWD
	return
    }

    foreach fname [lsort [glob -nocomplain * .*]] {
	if {![string compare . $fname]} {
	    continue
	}
	if [file isdirectory $data(-directory)/$fname] {
	    $data(w:dirlist) subwidget listbox insert end $fname
	}
    }

    # force glob to list the .* files. However, since the use might not
    # be interested in them, shift the listbox so that the "normal" files
    # are seen first
    #
    set top 0
    if {$data(-pattern) == "*"} {
	foreach fname [lsort [glob -nocomplain * .*]] {
	    if {![file isdirectory $data(-directory)/$fname]} {
		$data(w:filelist) subwidget listbox insert end $fname
		if [string match .* $fname] {
		    incr top
		}
	    }
	}
    } else {
	foreach fname [lsort [glob -nocomplain $data(-pattern)]] {
	    if {![file isdirectory $data(-directory)/$fname]} {
		$data(w:filelist) subwidget listbox insert end $fname
	    }
	}
    }

    $data(w:filelist) subwidget listbox yview $top
    cd $appPWD
}

proc tixFileSelectBox::LoadDir {w} {
    upvar #0 $w data

    tixBusy $w on [$data(w:dirlist) subwidget listbox]

    catch {
	# This will fail if a directory is not readable .... or some
	# strange reasons
	#
	tixFileSelectBox::LoadDirIntoLists $w
	tixFileSelectBox::MkDirMenu $w
    } err

    if {[$data(w:dirlist) subwidget listbox size] == 0} {
	$data(w:dirlist) subwidget listbox insert 0 ".."
    }


    tixWidgetDoWhenIdle tixBusy $w off [$data(w:dirlist) subwidget listbox]

#    if {$err != {}} {
#	error $err
#    }
}

# %% unimplemented
#
proc tixFileSelectBox::MkDirMenu {w} {
    upvar #0 $w data
}

proc tixFileSelectBox::SelectDir {w} {
    upvar #0 $w data

    if {$data(fakeDir) > 0} {
	incr data(fakeDir) -1
	$data(w:dirlist) subwidget listbox select clear 0 end
	$data(w:dirlist) subwidget listbox activate -1
	return
    }

    if {$data(flag)} {
	return
    }
    set data(flag) 1

    set subdir [tixListboxGetCurrent [$data(w:dirlist) subwidget listbox]]
    if {$subdir == {}} {
	set subdir "."
    }

    set filter \
	[tixFileSelectBox::GetFilter $w $data(-directory) \
         $subdir/$data(-pattern)]

    tixSetSilent $data(w:filter) $filter
    
    set data(flag) 0
}

proc tixFileSelectBox::InvokeDir {w} {
    upvar #0 $w data

    set theDir [$data(w:dirlist) subwidget listbox get active]

    set data(-directory) [tixResolveDir $data(-directory)/$theDir]
    $data(w:dirlist) subwidget listbox select clear 0 end

    tixFileSelectBox::InterpFilter $w \
	[tixFileSelectBox::GetFilter $w $data(-directory) $data(-pattern)]

    tixFileSelectBox::LoadDir $w

    if {![tixEvent match <Return>]} {
	incr data(fakeDir) 1
    }
}

proc tixFileSelectBox::SelectFile {w} {
    upvar #0 $w data

    if {$data(flag)} {
	return
    }
    set data(flag) 1

    # Reset the "Filter:" box to the current directory:
    #	
    $data(w:dirlist) subwidget listbox select clear 0 end
    set filter \
	[tixFileSelectBox::GetFilter $w $data(-directory) \
         $data(-pattern)]

    tixSetSilent $data(w:filter) $filter

    # Now select the file
    #
    set selected [tixListboxGetCurrent [$data(w:filelist) subwidget listbox]]
    if {$selected  != {}} {
	# Make sure that the selection is not empty!
	#
	if {$data(-directory) == "/"} {
	    tixSetSilent $data(w:selection) /$selected
	    set data(-value) /$selected
	} else {
	    tixSetSilent $data(w:selection) $data(-directory)/$selected
	    set data(-value) $data(-directory)/$selected
	}
	if {$data(-browsecmd) != {}} {
	    tixEvalCmdBinding $w $data(-browsecmd) {} \
		[$data(w:selection) cget -value]
	}
    }
    set data(flag) 0
}

proc tixFileSelectBox::InvokeFile {w} {
    upvar #0 $w data

    set selected [tixListboxGetCurrent [$data(w:filelist) subwidget listbox]]
    if {$selected  != {}} {
	$w invoke
    }
}

# This is only called the first this fileBox is mapped -- load the directory
#
proc tixFileSelectBox::FirstMapped {w} {
    if {![winfo exists $w]} {
	return
    }

    upvar #0 $w data

    tixFileSelectBox::LoadDir $w
    $data(w:filter) align
}


#----------------------------------------------------------------------
#
#
#              C O N V E N I E N C E   R O U T I N E S 
#
#
#----------------------------------------------------------------------

# This is obsolete. Use the widget tixFileSelectDialog instead
#
#
proc tixMkFileDialog {w args} {
    set option(-okcmd)    {}
    set option(-helpcmd)  {}

    tixHandleOptions option {-okcmd -helpcmd} $args

    toplevel $w
    wm minsize $w 10 10

    tixStdDlgBtns $w.btns
    
    if {$option(-okcmd) != {}} {
	tixFileSelectBox $w.fsb -command "wm withdraw $w; $option(-okcmd)"
    } else {
	tixFileSelectBox $w.fsb -command "wm withdraw $w"
    }

    $w.btns button ok     config -command "$w.fsb invoke"
    $w.btns button apply  config -command "$w.fsb filter" -text Filter
    $w.btns button cancel config -command "wm withdraw $w"

    if {$option(-helpcmd) == {}} {
	$w.btns button help config -state disabled
    } else {
	$w.btns button help config -command $option(-helpcmd)
    }
    wm protocol $w WM_DELETE_WINDOW "wm withdraw $w"
    pack $w.btns  -side bottom -fill both
    pack $w.fsb   -fill both -expand yes

    return $w.fsb
}


