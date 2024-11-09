# Tix Demostration Program
#
# This sample program is structured in such a way so that it can be
# executed from the Tix demo program "widget": it must have a
# procedure called "RunSample". It should also have the "if" statment
# at the end of this file so that it can be run as a standalone
# program using tixwish.

# This file demonstrates the use of the tixNoteBook widget, which allows
# you to lay out your interface using a "notebook" metaphore
#

proc RunSample {w} {

    # We use these options to set the sizes of the subwidgets inside the
    # notebook, so that they are well-aligned on the screen.
    #

    tixNoteBook $w.nb -ipadx 6 -ipady 6 -bg gray
    $w.nb subwidget nbframe config -backpagecolor gray

    $w.nb add hard_disk -label "Hard Disk" -underline 0
    $w.nb add network   -label "Network"   -underline 0
    place $w.nb -x 0 -y 0 -width 100 -height 100
}

if {![info exists tix_demo_running]} {
    wm withdraw .
    set w .demo
    toplevel $w
    RunSample $w
}
