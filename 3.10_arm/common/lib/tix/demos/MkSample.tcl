set tix_demo_running 1

set sample_types {widget image}

set sample_comments(widget) "Widget Demostrations"
set sample_comments(image) "Image Demostrations"


set samples_ignores {
    These are only alpha codes ...
    {"Drag and Drop"		DragDrop.tcl}
}

set samples(widget) {
    {Balloon			Balloon.tcl}
    {ButtonBox			BtnBox.tcl}
    {ComboBox			ComboBox.tcl}
    {Control			Control.tcl}
    {DirList			DirList.tcl}
    {DirTree			DirTree.tcl}
    {ExFileSelectDialog		EFileDlg.tcl}
    {FileSelectDialog		FileDlg.tcl}
    {FileEntry			FileEnt.tcl}
    {HList			HList1.tcl}
    {LabelEntry			LabEntry.tcl}
    {LabelFrame			LabFrame.tcl}
    {ListNoteBook		ListNBK.tcl}
    {NoteBook			NoteBook.tcl}
    {OptionMenu			OptMenu.tcl}
    {PanedWindow		PanedWin.tcl}
    {PopupMenu			PopMenu.tcl}
    {"ScrolledHList (1)"	SHList.tcl}
    {"ScrolledHList (2)"	SHList2.tcl}
    {ScrolledListBox		SListBox.tcl}
    {ScrolledText		SText.tcl}
    {ScrolledWindow		SWindow.tcl}
    {Select			Select.tcl}
    {StdButtonBox		StdBBox.tcl}
    {Tree			Tree.tcl}
    {"Tree (Dynamic)"		DynTree.tcl}
}

set samples(image) {
    {"Compound Image"			CmpImg.tcl}
    {"Compound Image (In NoteBook)"	CmpImg2.tcl}
    {"XPM Image"			Xpm.tcl}
    {"XPM Image (In Menu)"		Xpm1.tcl}
}

proc MkSample {nb page} {
    global tixOption
    set w [$nb subwidget $page]

    set pane [tixPanedWindow $w.pane -orient horizontal]
    pack $pane -expand yes -fill both

    set f1 [$pane add 1]
    set f2 [$pane add 2]
    $f1 config -relief flat
    $f2 config -relief flat

    # Left pane: the Tree:
    #
    set lab [label $f1.lab  -text "Select a sample program:" -anchor w]
    set tree [tixTree $f1.slb \
	-options {
	    hlist.selectMode single
	}]
    $tree config \
	-command   "Sample:Action $w $tree run" \
	-browsecmd "Sample:Action $w $tree browse"

    pack $lab -side top -fill x -padx 5 -pady 5
    pack $tree -side top -fill both -expand yes -padx 5

    # Right pane: the Text
    #
    set labe [tixLabelEntry $f2.lab -label "Source:" -options {
	label.anchor w
    }]

    $labe subwidget entry config -state disabled

    set stext [tixScrolledText $f2.stext]
    set f3 [frame $f2.f3]

    set run  [button $f3.run  -text "Run ..."  \
	-command "Sample:Action $w $tree run"]
    set view [button $f3.view -text "View Source in Browser ..." \
	-command "Sample:Action $w $tree view"]

    pack $run $view -side left -fill y -pady 2

    pack $labe -side top -fill x -padx 7 -pady 2
    pack $f3 -side bottom -fill x -padx 7
    pack $stext -side top -fill both -expand yes -padx 7

    #
    # Set up the text subwidget

    set text [$stext subwidget text]
    bind $text <1> "focus %W"
    bind $text <Up>    "%W yview scroll -1 unit"
    bind $text <Down>  "%W yview scroll 1 unit"
    bind $text <Left>  "%W xview scroll -1 unit"
    bind $text <Right> "%W xview scroll 1 unit"
    bind $text <Tab>   {focus [tk_focusNext %W]; break}

    bindtags $text "$text Text [winfo toplevel $text] all"

    $text config -bg [$tree subwidget hlist cget -bg] \
	-state disabled -font $tixOption(fixed_font) -wrap none

    $run  config -state disabled
    $view config -state disabled

    global demo
    set demo(w:run)  $run
    set demo(w:view) $view
    set demo(w:tree) $tree
    set demo(w:lab1) $labe
    set demo(w:stext) $stext

    AddSamples $tree
}

proc AddSamples {w} {
    global samples sample_types sample_comments

    set hlist [$w subwidget hlist]
    $hlist config -separator "." -width 25 -drawbranch 0 \
	-wideselect false

    set style [tixDisplayStyle imagetext -fg #202060 -padx 4]
    set index 0

    set icon   [tix getimage textfile]
    set folder [tix getimage openfolder]

    foreach type $sample_types {
	if {$index != 0} {
	    frame $hlist.sep$index -bd 2 -height 2 -width 150 -relief sunken \
		-bg [$hlist cget -bg]
	    $hlist addchild {} -itemtype window \
		-window $hlist.sep$index -state disabled
	}

	set p [$hlist addchild {} -itemtype imagetext\
	    -text $sample_comments($type) -style $style \
	    -image $folder]
	$w setmode $p close
	foreach sample $samples($type) {
	    $hlist addchild $p -itemtype imagetext \
		-data $sample -text [lindex $sample 0] -image $icon
	}
	incr index
    }

    $hlist select clear
}

set sample_filename {}

proc Sample:Action {w slb action args} {
    global samples demo_dir demo

    set hlist [$slb subwidget hlist]
    set ent [$hlist info anchor]

    if {$ent == ""} {
	$demo(w:run)  config -state disabled
	$demo(w:view) config -state disabled
	return
    }
    if {[$hlist info parent $ent] == {}} {
	# This is just a comment
	$demo(w:run)  config -state disabled
	$demo(w:view) config -state disabled
	return
    } else {
	$demo(w:run)  config -state normal
	$demo(w:view) config -state normal
    }

    set theSample [$hlist info data $ent]
    set title [lindex $theSample 0]
    set prog  [lindex $theSample 1]

    set samples_dir $demo_dir/samples

    case $action {
	"run" {
	    set w .[lindex [split $prog .] 0]
	    set w [string tolower $w]

	    if [winfo exists $w] {
		wm deiconify $w
		raise $w
		return
	    }

	    uplevel #0 source $samples_dir/$prog

	    toplevel $w 
	    wm title $w $title
	    RunSample $w
	}
	"view" {
	    LoadFile $samples_dir/$prog
	}
	"browse" {
	    # Bring up a short description of the sample program
	    # in the scrolled text about

	    set text [$demo(w:stext) subwidget text]
	    uplevel #0 set sample_filename $samples_dir/$prog
	    tixWidgetDoWhenIdle ReadFileWhenIdle $text

	    $demo(w:lab1) subwidget entry config -state normal
	    $demo(w:lab1) subwidget entry delete 0 end
	    $demo(w:lab1) subwidget entry insert end $samples_dir/$prog
	    $demo(w:lab1) subwidget entry xview end
	    $demo(w:lab1) subwidget entry config -state disabled
	}
    }
}

proc LoadFile {filename} {
    global tixOption

    set tmp $filename
    regsub -all . $filename _ tmp
    set w [string tolower .$tmp]

    if [winfo exists $w] {
	wm deiconify $w
	raise $w
	return
    }

    toplevel $w 
    wm title $w "Source View: $filename"

    button $w.b -text Close -command "destroy $w"
    set t [tixScrolledText $w.text]
    tixForm $w.b    -left 0 -bottom -0 -padx 4 -pady 4
    tixForm $w.text -left 0 -right -0 -top 0 -bottom $w.b

    $t subwidget text config -highlightcolor [$t cget -bg] -bd 2 \
	-bg [$t cget -bg] -font $tixOption(fixed_font) 
    if {$filename == {}} {
	return
    }

    set text [$w.text subwidget text]
    $text config -wrap none

    ReadFile $text $filename
}

proc ReadFileWhenIdle {text} {
    global sample_filename
    
    ReadFile $text $sample_filename
}

proc ReadFile {text filename} {
    set oldState [$text cget -state]
    $text config -state normal
    $text delete 0.0 end

    catch {
	set fd [open $filename {RDONLY}]
	$text delete 1.0 end

	while {![eof $fd]} {
	    $text insert end [gets $fd]\n
	}
	close $fd

	
    }

    $text see 1.0
    $text config -state $oldState
}
