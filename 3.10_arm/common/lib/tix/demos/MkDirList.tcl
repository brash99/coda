proc MkDirList {nb page} {
    set w [$nb subwidget $page]

    set name [tixOptionName $w]
    option add *$name*TixLabelFrame*label.padX 4

    tixLabelFrame $w.dir  -label "tixDirList"
    tixLabelFrame $w.fsbox -label "tixExFileSelectBox"
    MkDirListWidget [$w.dir subwidget frame]
    MkExFileWidget  [$w.fsbox subwidget frame]

    tixForm $w.dir  -top 0 -left 0 -right %40 -bottom -1
    tixForm $w.fsbox -top 0 -left %40 -right -1 -bottom -1
}

proc MkDirListWidget {w} {
    set name [tixOptionName $w]

    message $w.msg -font -*-helvetica-bold-r-normal-*-14-*-*-*-*-*-*-*\
	-relief flat -width 240 -anchor n\
	-text {The TixDirList widget gives a graphical representation of \
the file system directory and makes it easy for the user to choose and \
access directories.}

    tixDirList $w.dirlist -options {
	hlist.padY 1
	hlist.width 25
	hlist.height 16
    }

    pack $w.msg     -side top -expand yes -fill both -padx 3 -pady 3
    pack $w.dirlist -side top  -padx 3 -pady 3
}

proc MkExFileWidget {w} {
    set name [tixOptionName $w]

    message $w.msg -font -*-helvetica-bold-r-normal-*-14-*-*-*-*-*-*-*\
	-relief flat -width 240 -anchor n\
	-text {The TixExFileSelectBox widget is more user friendly \
than the Motif style FileSelectBox.}


    tixExFileSelectBox $w.exfsbox -bd 2 -relief raised

    pack $w.msg    -side top -expand yes -fill both -padx 3 -pady 3
    pack $w.exfsbox -side top  -padx 3 -pady 3
}

