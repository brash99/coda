# Tix Demostration Program
#
# This sample program is structured in such a way so that it can be
# executed from the Tix demo program "widget": it must have a
# procedure called "RunSample". It should also have the "if" statment
# at the end of this file so that it can be run as a standalone
# program using tixwish.

# This file demonstrates how to use the compound image inside NoteBook
# widgets. This file is basically a cross-over of NoteBook.tcl and CmpImg.tcl
#
proc RunSample {w} {

    # Create the notebook widget and set its backpagecolor to gray.
    # Note that the -backpagecolor option belongs to the "nbframe"
    # subwidget.
    tixNoteBook $w.nb -ipadx 6 -ipady 6
    $w config -bg gray
    $w.nb subwidget nbframe config -backpagecolor gray -tabpady 0

    # Create the two compound images
    #
    #
    if {[winfo depth .] < 8} {
	# Use two color bitmaps
	#	
	global network_bitmap hard_disk_bitmap

	set img0 [image create bitmap -data $network_bitmap]
	set img1 [image create bitmap -data $hard_disk_bitmap]
    } else {
	# Use color pixmaps
	#	
	global network_pixmap hard_disk_pixmap
	set img0 [image create pixmap -data $network_pixmap]
	set img1 [image create pixmap -data $hard_disk_pixmap]
    }

    # Create the first image:
    #
    # Notice that the -window option must be set to the nbframe
    # subwidget of the notebook because the image will be displayed
    # in that widget.
    #
    set hdd_img [image create compound -window [$w.nb subwidget nbframe] \
	-pady 0]
    $hdd_img add line
    $hdd_img add image -image $img1
    $hdd_img add space -width 7
    $hdd_img add text -text "Hard Disk" -underline 0
 
    # Create the second compound image. Very similar to what we did above
    #
    set net_img [image create compound -window [$w.nb subwidget nbframe] \
	-pady 0]
    $net_img add line
    $net_img add image -image $img0
    $net_img add space -width 7
    $net_img add text -text "Network" -underline 0

    #
    # Now create the pages
    #

    # We use these options to set the sizes of the subwidgets inside the
    # notebook, so that they are well-aligned on the screen.
    #
    set name [tixOptionName $w]
    option add *$name*TixControl*entry.width 10
    option add *$name*TixControl*label.width 18
    option add *$name*TixControl*label.anchor e

    # Create the two tabs on the notebook. The -underline option
    # puts a underline on the first character of the labels of the tabs.
    # Keyboard accelerators will be defined automatically according
    # to the underlined character.	
    #
    $w.nb add hard_disk -image $hdd_img
    $w.nb add network   -image $net_img
    pack $w.nb -expand yes -fill both -padx 5 -pady 5 -side top

    #----------------------------------------
    # Create the first page
    #----------------------------------------
    set f [$w.nb subwidget hard_disk]

    # Create two frames: one for the common buttons, one for the
    # other widgets
    #
    frame $f.f
    frame $f.common
    pack $f.f      -side left  -padx 2 -pady 2 -fill both -expand yes
    pack $f.common -side right -padx 2 -pady 2 -fill y

    # Create the controls that only belong to this page
    #
    tixControl $f.f.a -value 12   -label "Access Time: "
    tixControl $f.f.w -value 400  -label "Write Throughput: "
    tixControl $f.f.r -value 400  -label "Read Throughput: "
    tixControl $f.f.c -value 1021 -label "Capacity: "
    pack $f.f.a $f.f.w $f.f.r $f.f.c  -side top -padx 20 -pady 2

    # Create the common buttons
    #
    CreateCommonButtons $w $f.common
    
    #----------------------------------------
    # Create the second page	
    #----------------------------------------
    set f [$w.nb subwidget network]

    frame $f.f
    frame $f.common
    pack $f.f      -side left  -padx 2 -pady 2 -fill both -expand yes
    pack $f.common -side right -padx 2 -pady 2 -fill y

    tixControl $f.f.a -value 12   -label "Access Time: "
    tixControl $f.f.w -value 400  -label "Write Throughput: "
    tixControl $f.f.r -value 400  -label "Read Throughput: "
    tixControl $f.f.c -value 1021 -label "Capacity: "
    tixControl $f.f.u -value 10   -label "Users: "

    pack $f.f.a $f.f.w $f.f.r $f.f.c $f.f.u -side top -padx 20 -pady 2

    CreateCommonButtons $w $f.common
}

proc CreateCommonButtons {w f} {
    button $f.ok     -text OK     -width 6 -command "destroy $w"
    button $f.cancel -text Cancel -width 6 -command "destroy $w"

    pack $f.ok $f.cancel -side top -padx 2 -pady 2
}


set network_pixmap {/* XPM */
static char * netw_xpm[] = {
/* width height ncolors chars_per_pixel */
"32 31 7 1",
/* colors */
" 	s None	c None",
".	c #000000000000",
"X	c white",
"o	c #c000c000c000",
"O	c #404040",
"+	c blue",
"@	c red",
/* pixels */
"                                ",
"                 .............. ",
"                 .XXXXXXXXXXXX. ",
"                 .XooooooooooO. ",
"                 .Xo.......XoO. ",
"                 .Xo.++++o+XoO. ",
"                 .Xo.++++o+XoO. ",
"                 .Xo.++oo++XoO. ",
"                 .Xo.++++++XoO. ",
"                 .Xo.+o++++XoO. ",
"                 .Xo.++++++XoO. ",
"                 .Xo.XXXXXXXoO. ",
"                 .XooooooooooO. ",
"                 .Xo@ooo....oO. ",
" ..............  .XooooooooooO. ",
" .XXXXXXXXXXXX.  .XooooooooooO. ",
" .XooooooooooO.  .OOOOOOOOOOOO. ",
" .Xo.......XoO.  .............. ",
" .Xo.++++o+XoO.        @        ",
" .Xo.++++o+XoO.        @        ",
" .Xo.++oo++XoO.        @        ",
" .Xo.++++++XoO.        @        ",
" .Xo.+o++++XoO.        @        ",
" .Xo.++++++XoO.      .....      ",
" .Xo.XXXXXXXoO.      .XXX.      ",
" .XooooooooooO.@@@@@@.X O.      ",
" .Xo@ooo....oO.      .OOO.      ",
" .XooooooooooO.      .....      ",
" .XooooooooooO.                 ",
" .OOOOOOOOOOOO.                 ",
" ..............                 ",};}

set hard_disk_pixmap {/* XPM */
static char * drivea_xpm[] = {
/* width height ncolors chars_per_pixel */
"32 31 5 1",
/* colors */
" 	s None	c None",
".	c #000000000000",
"X	c white",
"o	c #c000c000c000",
"O	c #800080008000",
/* pixels */
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"   ..........................   ",
"   .XXXXXXXXXXXXXXXXXXXXXXXo.   ",
"   .XooooooooooooooooooooooO.   ",
"   .Xooooooooooooooooo..oooO.   ",
"   .Xooooooooooooooooo..oooO.   ",
"   .XooooooooooooooooooooooO.   ",
"   .Xoooooooo.......oooooooO.   ",
"   .Xoo...................oO.   ",
"   .Xoooooooo.......oooooooO.   ",
"   .XooooooooooooooooooooooO.   ",
"   .XooooooooooooooooooooooO.   ",
"   .XooooooooooooooooooooooO.   ",
"   .XooooooooooooooooooooooO.   ",
"   .oOOOOOOOOOOOOOOOOOOOOOOO.   ",
"   ..........................   ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                ",
"                                "};}

set network_bitmap {
#define netw_width 32
#define netw_height 32
static unsigned char netw_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0x02, 0x40,
   0x00, 0x00, 0xfa, 0x5f, 0x00, 0x00, 0x0a, 0x50, 0x00, 0x00, 0x0a, 0x52,
   0x00, 0x00, 0x0a, 0x52, 0x00, 0x00, 0x8a, 0x51, 0x00, 0x00, 0x0a, 0x50,
   0x00, 0x00, 0x4a, 0x50, 0x00, 0x00, 0x0a, 0x50, 0x00, 0x00, 0x0a, 0x50,
   0x00, 0x00, 0xfa, 0x5f, 0x00, 0x00, 0x02, 0x40, 0xfe, 0x7f, 0x52, 0x55,
   0x02, 0x40, 0xaa, 0x6a, 0xfa, 0x5f, 0xfe, 0x7f, 0x0a, 0x50, 0xfe, 0x7f,
   0x0a, 0x52, 0x80, 0x00, 0x0a, 0x52, 0x80, 0x00, 0x8a, 0x51, 0x80, 0x00,
   0x0a, 0x50, 0x80, 0x00, 0x4a, 0x50, 0x80, 0x00, 0x0a, 0x50, 0xe0, 0x03,
   0x0a, 0x50, 0x20, 0x02, 0xfa, 0xdf, 0x3f, 0x03, 0x02, 0x40, 0xa0, 0x02,
   0x52, 0x55, 0xe0, 0x03, 0xaa, 0x6a, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00,
   0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
}

set hard_disk_bitmap {
#define drivea_width 32
#define drivea_height 32
static unsigned char drivea_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0xf8, 0xff, 0xff, 0x1f, 0x08, 0x00, 0x00, 0x18, 0xa8, 0xaa, 0xaa, 0x1a,
   0x48, 0x55, 0xd5, 0x1d, 0xa8, 0xaa, 0xaa, 0x1b, 0x48, 0x55, 0x55, 0x1d,
   0xa8, 0xfa, 0xaf, 0x1a, 0xc8, 0xff, 0xff, 0x1d, 0xa8, 0xfa, 0xaf, 0x1a,
   0x48, 0x55, 0x55, 0x1d, 0xa8, 0xaa, 0xaa, 0x1a, 0x48, 0x55, 0x55, 0x1d,
   0xa8, 0xaa, 0xaa, 0x1a, 0xf8, 0xff, 0xff, 0x1f, 0xf8, 0xff, 0xff, 0x1f,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
}

if {![info exists tix_demo_running]} {
    wm withdraw .
    set w .demo
    toplevel $w
    RunSample $w
}
