# ListNBk.tcl --
#
#	"List NoteBook" widget. Acts similarly to the notebook but uses a
#	HList widget to represent the pages.
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

tixWidgetClass tixListNoteBook {
    -classname tixListNoteBook
    -superclass tixVStack
    -method {
    }
    -flag {
    }
    -configspec {
    }
}

proc tixListNoteBook::ConstructWidget {w} {
    upvar #0 $w data

    set data(w:shlist) [tixScrolledHList $w.shlist]
    set data(w:hlist) [$data(w:shlist) subwidget hlist]

    $data(w:hlist) config \
	-command   "tixListNoteBook::Choose $w"\
	-browsecmd "tixListNoteBook::Choose $w"\
	-selectmode single

    # We can't use the packer because it will conflict with the
    # geometry management of the VStack widget.
    #
    tixManageGeometry $data(w:shlist) "tixVStack::MasterGeomProc $w"
}

proc tixListNoteBook::add {w child args} {
    upvar #0 $w data

    if [string match *.* $child] {
	error "the name of the page cannot contain the \".\" character"
    }
    return [eval tixChainMethod $w add $child $args]
}

proc tixListNoteBook::raise {w child} {
    upvar #0 $w data

    $data(w:hlist) selection clear
    $data(w:hlist) selection set $child
    $data(w:hlist) anchor set $child

    tixChainMethod $w raise $child
}

proc tixListNoteBook::Choose {w args} {
    upvar #0 $w data
 
    set entry [tixEvent flag V]

    if {[lsearch $data(windows) $entry] != -1} {
	tixCallMethod $w raise $entry
    }
}

proc tixListNoteBook::Resize {w} {
    upvar #0 $w data

    # We have to take care of the size of the tabs so that 
    #
    set tW [winfo reqwidth  $data(w:shlist)]
    set tH [winfo reqheight $data(w:shlist)]

    tixMoveResizeWindow $data(w:shlist) $data(-ipadx) $data(-ipady) $tW $tH
    tixMapWindow $data(w:shlist)

    set data(pad-x1) $tW
    set data(minW)   [expr $tW + 2 * $data(-ipadx)]
    set data(minH)   [expr $tH + 2 * $data(-ipady)]

    # Now that we know data(pad-y1), we can chain the call
    #
    tixChainMethod $w Resize
}
