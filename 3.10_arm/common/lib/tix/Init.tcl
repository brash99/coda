# Init.tcl --
#
#	Initializes the Tix library
#
# Copyright (c) 1996, Expert Interface Technologies
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


# STEP 1: Version checking
#
#
if [catch {
    if {[tixScriptVersion] != $tix_version} {
        puts stderr "error: Tix library version ([tixScriptVersion]) does not match binary version ($tix_version)"
	error 2
    }
    if {[tixScriptPatchLevel] != $tix_patchLevel} {
        puts stderr "error: Tix library patch-level ([tixScriptPatchLevel]) does not match binary patch-level ($tix_patchLevel)"
	error 1
    }
} err ]  {
    puts stderr "$err\n  Please check your TIX_LIBRARY environment variable and your Tix installaion."
}
    
# STEP 2: Initialize the Tix application context
#
#
if {![info exists tix]} {
    tixAppContext::AutoLoad
}

# STEP 3: Initialize the bindings for widgets that are implemented in C
#
#
tixHListBindSingle
tixHListBindMultiple

# STEP 4: Some ITCL compatibility stuff
#
#

if {[info command "@scope"] != {}} {
    rename update __update

    proc update {args} {
	@scope :: eval __update $args
    }

    rename tkwait __tkwait

    proc tkwait {args} {
	@scope :: eval __tkwait $args
    }
}
