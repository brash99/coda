# Tix Demostration Program
#
# This sample program is structured in such a way so that it can be
# executed from the Tix demo program "widget": it must have a
# procedure called "RunSample". It should also have the "if" statment
# at the end of this file so that it can be run as a standalone
# program using tixwish.

# This program demonstrates the ListBoteBook widget, which is very similar
# to a NoteBook widget but uses an HList instead of page tabs to list the
# pages.

proc RunSample {w} {
    set top [frame $w.f -bd 1 -relief raised]
    set box [tixButtonBox $w.b -bd 1 -relief raised]

    pack $box -side bottom -fill both
    pack $top -side top -fill both -expand yes

    #----------------------------------------------------------------------
    # Create the ListNoteBook with nice icons
    #----------------------------------------------------------------------
    tixListNoteBook $top.n -ipadx 6 -ipady 6

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

    $top.n subwidget hlist add hard_disk -itemtype imagetext \
	-image $img0 -text "Hard Disk" -under 0
    $top.n subwidget hlist add network   -itemtype imagetext \
	-image $img1 -text "Network"   -under 0

    $top.n add hard_disk 
    $top.n add network
    
    #
    # Create the widgets inside the two pages
    
    # We use these options to set the sizes of the subwidgets inside the
    # notebook, so that they are well-aligned on the screen.
    #
    set name [tixOptionName $w]
    option add *$name*TixControl*entry.width 10
    option add *$name*TixControl*label.width 18
    option add *$name*TixControl*label.anchor e

    set f [$top.n subwidget hard_disk]

    tixControl $f.a -value 12   -label "Access Time: "
    tixControl $f.w -value 400  -label "Write Throughput: "
    tixControl $f.r -value 400  -label "Read Throughput: "
    tixControl $f.c -value 1021 -label "Capacity: "
    pack $f.a $f.w $f.r $f.c  -side top -padx 20 -pady 2

    set f [$top.n subwidget network]

    tixControl $f.a -value 12   -label "Access Time: "
    tixControl $f.w -value 400  -label "Write Throughput: "
    tixControl $f.r -value 400  -label "Read Throughput: "
    tixControl $f.c -value 1021 -label "Capacity: "
    tixControl $f.u -value 10   -label "Users: "

    pack $f.a $f.w $f.r $f.c $f.u -side top -padx 20 -pady 2

    pack $top.n -expand yes -fill both -padx 5 -pady 5

    # Create the buttons
    #
    $box add ok     -text Ok     -command "destroy $w" -width 6
    $box add cancel -text Cancel -command "destroy $w" -width 6
}


#----------------------------------------------------------------------
# Some icons
#----------------------------------------------------------------------
set network_pixmap {/* XPM */
static char * netw_xpm[] = {
/* width height ncolors chars_per_pixel */
"32 32 7 1",
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
" ..............                 ",
"                                "};}

set hard_disk_pixmap {/* XPM */
static char * drivea_xpm[] = {
/* width height ncolors chars_per_pixel */
"32 32 5 1",
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

#----------------------------------------------------------------------
# Start-up code
#----------------------------------------------------------------------

if {![info exists tix_demo_running]} {
    wm withdraw .
    set w .demo
    toplevel $w
    RunSample $w
    bind $w <Destroy> exit
}
