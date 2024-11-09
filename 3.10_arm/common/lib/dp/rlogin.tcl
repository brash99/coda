proc rlogin {target} {
    while {1} {
	puts -nonewline "$targ% "
	set cmd  [gets file0]
	if { $cmd == "logout" } break
	set tcmd "ask $targ $cmd"
	catch {eval $tcmd} res
	puts $res
    }
}
