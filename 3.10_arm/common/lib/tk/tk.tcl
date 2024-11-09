# tk.tcl --
#
# Initialization script normally executed in the interpreter for each
# Tk-based application.  Arranges class bindings for widgets.
#
# @(#) tk.tcl 1.73 95/08/30 16:40:20
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# ========================================================================
# >>>>>>>>>>>>>>>> INCLUDES MODIFICATIONS FOR [incr Tcl] <<<<<<<<<<<<<<<<<
#
#  AUTHOR:  Michael J. McLennan       Phone: (610)712-2842
#           AT&T Bell Laboratories   E-mail: michael.mclennan@att.com
#     RCS:  $Id: tk.tcl,v 1.1.1.1 1996/08/21 19:27:26 heyes Exp $
# ========================================================================
#             Copyright (c) 1993-1995  AT&T Bell Laboratories
# ------------------------------------------------------------------------
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Insist on running with compatible versions of Tcl and Tk.

scan [info tclversion] "%d.%d" a b
if {$a != 7} {
    error "wrong version of Tcl loaded ([info tclversion]): need 7.x"
}
scan $tk_version "%d.%d" a b
if {($a != 4) || ($b < 0)} {
    error "wrong version of Tk loaded ($tk_version): need 4.x"
}
unset a b

# Add Tk's directory to the end of the auto-load search path:

lappend auto_path $tk_library

# Turn off strict Motif look and feel as a default.

set tk_strictMotif 0

# ------------------------------------------------------------------------
#  USAGE: tk_unknown ?command arg arg...?
#
#  Unknown command handler for Tk.  Checks to see if "command" is
#  the name of a Tk window with a scoped command.  If it is, and if
#  the "itcl::purist" flag is not set, then the scoped command is
#  invoked automatically to access the widget.  This provides backward
#  compatibility for Tcl/Tk applications that assume all widgets are
#  accessible from the global scope.
#
#  When the "itcl::purist" mode is set, widgets are protected by the
#  namespace that contains them, and must be invoked using a scoped
#  command.
#
#  If the "command" is not a window name, then this procedure returns
#  "-code continue", telling the unknown facility to try another
#  handler.
# ------------------------------------------------------------------------
namespace itcl {
    public variable purist 0

    public proc tk_unknown {args} {
        global purist errorInfo

        set win [lindex $args 0]
	set temp $errorInfo
        if {[catch {winfo command $win} cmd] == 0 &&
            $cmd != "" && !$purist} {
	    set errorInto $temp
            return [uplevel $cmd [lrange $args 1 end]]
        }
	set errorInfo $temp
        return -code continue
    }
    unknown_handler "tk" itcl::tk_unknown
}

# tkScreenChanged --
# This procedure is invoked by the binding mechanism whenever the
# "current" screen is changing.  The procedure does two things.
# First, it uses "upvar" to make global variable "tkPriv" point at an
# array variable that holds state for the current display.  Second,
# it initializes the array if it didn't already exist.
#
# Arguments:
# screen -		The name of the new screen.

proc tkScreenChanged screen {
    set disp [file rootname $screen]
    uplevel #0 upvar #0 tkPriv.$disp tkPriv
    global tkPriv
    if [info exists tkPriv] {
	set tkPriv(screen) $screen
	return
    }
    set tkPriv(afterId) {}
    set tkPriv(buttons) 0
    set tkPriv(buttonWindow) {}
    set tkPriv(dragging) 0
    set tkPriv(focus) {}
    set tkPriv(grab) {}
    set tkPriv(initPos) {}
    set tkPriv(inMenubutton) {}
    set tkPriv(listboxPrev) {}
    set tkPriv(mouseMoved) 0
    set tkPriv(oldGrab) {}
    set tkPriv(popup) {}
    set tkPriv(postedMb) {}
    set tkPriv(pressX) 0
    set tkPriv(pressY) 0
    set tkPriv(screen) $screen
    set tkPriv(selectMode) char
    set tkPriv(window) {}
}

# Do initial setup for tkPriv, so that it is always bound to something
# (otherwise, if someone references it, it may get set to a non-upvar-ed
# value, which will cause trouble later).

tkScreenChanged [winfo screen .]

# ----------------------------------------------------------------------
# Read in files that define all of the class bindings.
# ----------------------------------------------------------------------

source $tk_library/button.tcl
source $tk_library/entry.tcl
source $tk_library/listbox.tcl
source $tk_library/menu.tcl
source $tk_library/scale.tcl
source $tk_library/scrollbar.tcl
source $tk_library/text.tcl

# ----------------------------------------------------------------------
# Default bindings for keyboard traversal.
# ----------------------------------------------------------------------

bind all <Tab> {focus [tk_focusNext %W]}
bind all <Shift-Tab> {focus [tk_focusPrev %W]}

# tkCancelRepeat --
# This procedure is invoked to cancel an auto-repeat action described
# by tkPriv(afterId).  It's used by several widgets to auto-scroll
# the widget when the mouse is dragged out of the widget with a
# button pressed.
#
# Arguments:
# None.

proc tkCancelRepeat {} {
    global tkPriv
    after cancel $tkPriv(afterId)
    set tkPriv(afterId) {}
}
