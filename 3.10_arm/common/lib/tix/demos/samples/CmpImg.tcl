# Tix Demostration Program
#
# This sample program is structured in such a way so that it can be
# executed from the Tix demo program "widget": it must have a
# procedure called "RunSample". It should also have the "if" statment
# at the end of this file so that it can be run as a standalone
# program using tixwish.

# This file demonstrates the use of the compound images: it uses compound
# images to display a text string together with a pixmap inside
# buttons
#
proc RunSample {w} {
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

    button $w.hdd -padx 4 -pady 1 -width 120
    button $w.net -padx 4 -pady 1 -width 120

    # Create the first image: we create a line, then put a string,
    # a space and a image into this line, from left to right.
    # The result: we have a one-line image that consists of three
    # individual items
    #
    set hdd_img [image create compound -window $w.hdd]
    $hdd_img add line
    $hdd_img add text -text "Hard Disk" -underline 0
    $hdd_img add space -width 7
    $hdd_img add image -image $img1
 
    # Put this image into the first button
    #
    $w.hdd config -image $hdd_img

    # Create the second compound image. Very similar to what we did above
    #
    set net_img [image create compound -window $w.net]
    $net_img add line
    $net_img add text -text "Network" -underline 0
    $net_img add space -width 7
    $net_img add image -image $img0

    $w.net config -image $net_img

    # The button to close the window
    #

    button $w.clo -pady 1 -text Close -command "destroy $w"

    pack $w.hdd $w.net $w.clo -side left -padx 10 -pady 10 -fill y -expand yes
}

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

if {![info exists tix_demo_running]} {
    wm withdraw .
    set w .demo
    toplevel $w
    RunSample $w
}
