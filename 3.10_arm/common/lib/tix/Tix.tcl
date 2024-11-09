# Tix.tcl --
#
#	This file implements the Tix application context class
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


tixClass tixAppContext {
    -superclass {}
    -classname  TixAppContext
    -method {
	cget configure addbitmapdir filedialog getbitmap getimage
	option resetoptions setbitmap
    }
    -flag {
	-binding -debug -extracmdargs -filedialog -fontset -grabmode
	-haspixmap -libdir -scheme -schemepriority -percentsubst
    }
    -readonly {
	-haspixmap
    }
    -configspec {
	{-binding    		TK}
	{-debug      		false}
	{-extracmdargs 		1}
	{-filedialog    	{}}
	{-fontset    		TK}
	{-grabmode 		global}
	{-haspixmap 		0}
	{-libdir     		{}}
	{-percentsubst		0}
	{-scheme     		TK}
	{-schemepriority     	21}
    }
    -alias {
    }
}

proc tixAppContext::Constructor {w} {
    upvar #0 $w data
    global tix_priv env argv0
    global tix_library tixOption

    if {$tix_library == "nowhere"} {
	set data(et) 1
    } else {
	set data(et) 0
    }

    set data(isStartUp) 1
    # Thses options were set when tixwish started up
    #
    set data(-binding)		$tix_priv(-binding)
    set data(-debug)		$tix_priv(-debug)
    set data(-fontset)		$tix_priv(-fontset)
    set data(-libdir)		$tix_priv(-libdir)
    set data(-scheme)		$tix_priv(-scheme)
    set data(-schemepriority)	$tix_priv(-schemepriority)
    set data(-libdir)		$tix_library
    set tixOption(prioLevel) $tix_priv(-schemepriority)
	
    # Enable/Disable Intrinsics debugging
    #
    if {$data(-debug)} {
	set tix_priv(debug) 1
    } else {
	set tix_priv(debug) 0
    }

    tixAppContext::config-fontset $w $data(-fontset)
    tixAppContext::config-scheme  $w $data(-scheme)

    tixAppContext::BitmapInit $w
    tixAppContext::FileDialogInit $w

    if {[tixGetBoolean -nocomplain $data(-debug)]} {
	# For widget programming, it is more convient to have the error
	# message printed on the terminal. For some extensive usage of
	# bindings, suce as in the case of tixBalloon, the default
	# therror just doesn't work.
	#
	proc ___tkerror {err} {
	    global errorInfo
	    puts $err
	    puts $errorInfo
	}
    }

    # Force the "." window to accept the new Tix options
    #
    foreach spec [. configure] {
	if {[llength $spec] > 2} {
	    set flag  [lindex $spec 0]
	    set name  [lindex $spec 1]
	    set class [lindex $spec 2]
	    set value [option get . $name $class]
	    catch {. config $flag $value}
	}
    }
    set data(isStartUp) 0

    # Some hack: if env(TIX_DEBUG_INTERACTIVE) is set, then
    # an interactive prompt is always printed
    if [info exists env(TIX_DEBUG_INTERACTIVE)] {
	if {![string match *tixwish* $argv0]} {
	    # Looks like this was not started up as an interactive shell
	    # Let's open an interactive input from stdin
	    tixShellInput
	}
    }

    if [info exists env(TIX_DEBUG_GEOMETRY)] {
	if {![string match *tixwish* $argv0]} {
	    wm geometry . $env(TIX_DEBUG_GEOMETRY)
	}
    }
}

#----------------------------------------------------------------------
#  Configurations
#
#----------------------------------------------------------------------
proc tixAppContext::resetoptions {w scheme fontset {schemePrio {}}} {
    upvar #0 $w data

    if {! $data(et)} {
	global tixOption
	option clear

	if {$schemePrio != {}} {
	    set tixOption(prioLevel) $schemePrio
	}
	tixAppContext::config-scheme  $w $scheme
	tixAppContext::config-fontset $w $fontset
    }
}

proc tixAppContext::config-fontset {w value} {
    upvar #0 $w data
    global tix_priv tixOption

    set data(-fontset) $value

    #-----------------------------------
    # Initialization of options database
    #-----------------------------------
    # Load the fontset
    #
    if {!$data(et)} {
	if [file exists $data(-libdir)/pref/$data(-fontset).fsc] {
	    source $data(-libdir)/pref/$data(-fontset).fsc
	    tixPref:InitFontSet:$data(-fontset)
	    tixAppContext::CheckFontSets $w
	    tixPref:SetFontSet:$data(-fontset)
	} else {
	    puts stderr "\aError: cannot use fontset \"$data(-fontset)\""
	    puts stderr "       Using default fontset "
	    tixSetDefaultFontset
	    tixAppContext::CheckFontSets $w
	}
    } else {
	if [catch {
	    tixPref:InitFontSet:$data(-fontset)
	    tixAppContext::CheckFontSets $w
	    tixPref:SetFontSet:$data(-fontset)
	}] {
	    # User chose non-existent fontset
	    #
	    puts stderr "\aError: cannot use fontset \"$data(-fontset)\""
	    puts stderr "       Using default fontset "
	    tixSetDefaultFontset
	    tixAppContext::CheckFontSets $w
	}
    }

    # Compatibility stuff: the bsolete name courier_font has been changed to
    # fixed_font
    set tixOption(courier_font) $tixOption(fixed_font)
}

proc tixAppContext::config-scheme {w value} {
    upvar #0 $w data
    global tix_priv

    set data(-scheme) $value

    # Load the color scheme
    #
    if {!$data(et)} {
	if [file exists $data(-libdir)/pref/$data(-scheme).csc] {
	    source $data(-libdir)/pref/$data(-scheme).csc
	    if {[winfo depth .] >= 8} {
		tixPref:SetScheme-Color:$data(-scheme)
	    } else {
		tixPref:SetScheme-Mono:$data(-scheme)
	    }
	} else {
	    puts stderr "\aError: cannot use color scheme \"$data(-scheme)\""
	    puts stderr "       Using default color scheme"
	    if {[winfo depth .] >= 8} {
		tixSetDefaultScheme-Color
	    } else {
		tixSetDefaultScheme-Mono
	    }
	}
    } else {
	if [catch {
	    if {[winfo depth .] >= 8} {
		tixPref:SetScheme-Color:$data(-scheme)
	    } else {
		tixPref:SetScheme-Mono:$data(-scheme)
	    }
	}] {
	    # User chose non-existent color scheme
	    #
	    puts stderr "\aError: cannot use color scheme \"$data(-scheme)\""
	    puts stderr "       Using default color scheme"
	    if {[winfo depth .] >= 8} {
		tixSetDefaultScheme-Color
	    } else {
		tixSetDefaultScheme-Mono
	    }
	}
    }
}

#----------------------------------------------------------------------
#  Private methods
#
#----------------------------------------------------------------------
proc tixAppContext::BitmapInit {w} {
    upvar #0 $w data

    # See whether we have pixmap extension
    #
    set data(-haspixmap) true

    # Dynamically set the bitmap directory
    #
    if {! $data(et)} {
	set data(bitmapdirs) $data(-libdir)/bitmaps
    } else {
	set data(bitmapdirs) {}
    }
}

proc tixAppContext::FileDialogInit {w} {
    upvar #0 $w data

    if {$data(-filedialog) == {}} {
	set data(-filedialog) [option get . fileDialog FileDialog]
    }
    if {$data(-filedialog) == {}} {
	set data(-filedialog) tixFileSelectDialog
    }
}

#----------------------------------------------------------------------
# If a font in the fontset is not available, use a default fontset.
#
proc tixAppContext::CheckFontSets  {w} {
    upvar #0 $w data
    global tixOption

    set default_font "fixed"
    set options {font bold_font menu_font italic_font fixed_font}

    set lab [label .tix-xxx-test]
    foreach opt $options {
	if [catch {$lab config -font $tixOption($opt)}] {
	    puts stderr \
		"\aError: cannot use font \"$tixOption($opt)\" as \"$opt\""
	    puts  stderr \
		"       using \"$default_font\" instead"

	    set tixOption($opt) $default_font
	}
    }
    destroy $lab
}

#----------------------------------------------------------------------
# 	Public methods
#----------------------------------------------------------------------
proc tixAppContext::addbitmapdir {w bmpdir} {
    upvar #0 $w data

    if {[lsearch $data(bitmapdirs) $bmpdir] == "-1"} {
	set data(bitmapdirs) [concat $bmpdir $data(bitmapdirs)]
    }
}

proc tixAppContext::getimage {w name} {
    upvar #0 $w data

    if {[info exists data(img:$name)]} {
	return $data(img:$name)
    }

    foreach dir $data(bitmapdirs) {
	if [file exists $dir/$name.xpm] {
	    if {![catch {
		set data(img:$name) \
		    [image create pixmap -file $dir/$name.xpm]
	    }]} {
		break
	    }
	}
	if [file exists $dir/$name.xbm] {
	    if {![catch {
		set data(img:$name) \
		    [image create bitmap -file $dir/$name.xbm]
	    }]} {
		break
	    }
	}
	if [file exists $dir/$name] {
	    if {![catch {
		set data(img:$name) \
		    [image create bitmap -file $dir/$name]
	    }]} {
		break
	    }
	}
    }

    if {![info exists data(img:$name)]} {
	catch {
	    # This is for compiled-in images
	    set data(img:$name) [image create pixmap -id $name]
	} err
	if [string match internal* $err] {
	    error $err
	}
    }

    if {[info exists data(img:$name)]} {
	return $data(img:$name)
    } else {
	error "image file \"$name\" cannot be found"
    }
}


proc tixAppContext::getbitmap {w bitmapname} {
    upvar #0 $w data

    if {[info exists data(bmp:$bitmapname)]} {
	return $data(bmp:$bitmapname)
    } else {
	set ext [file extension $bitmapname]
	if {$ext == ""} {
	    set ext .xbm
	}

	# This is the fallback value. If we can't find the bitmap in
	# the bitmap directories, then use the name of the bitmap
	# as the default value.
	#
	set data(bmp:$bitmapname) $bitmapname

	foreach dir $data(bitmapdirs) {
	    case $ext {
		.ppm {
		    if [file exists $dir/$bitmapname] {
			set data(bmp:$bitmapname) \
			    [image create photo -file $dir/$bitmapname]
			break
		    }
		}
		.xbm {
		    if [file exists $dir/$bitmapname.xbm] {
			set data(bmp:$bitmapname) @$dir/$bitmapname.xbm
			break
		    }
		    if [file exists $dir/$bitmapname] {
			set data(bmp:$bitmapname) @$dir/$bitmapname
			break
		    }
		}
		default {
		    if [file exists $dir/$bitmapname] {
			set data(bmp:$bitmapname) @$dir/$bitmapname
			break
		    }
		}
	    }
	}

	return $data(bmp:$bitmapname)
    }
}

proc tixAppContext::filedialog {w {type tixFileSelectDialog}} {
    upvar #0 $w data

    if {$type == {}} {
	set type $data(-filedialog)
    }
    if {![info exists data(filedialog,$type)]} {
	set data(filedialog,$type) {}
    }

    if {$data(filedialog,$type) == {} || ![winfo exists $data(filedialog,$type)]} {
	set data(filedialog,$type) [$type .tixapp_filedialog_$type]
    }

    return $data(filedialog,$type)
}

proc tixAppContext::option {w action option {value {}}} {
    upvar #0 $w data
    global tixOption

    if {$action == "get"} {
	return $tixOption($option)
    }
}

tixAppContext tix
