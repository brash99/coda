if {"[info command TS]"!="TS"} {
    global tcl_modules

    lappend tcl_modules "TS_class.tcl {} {} {\$Id: TS_class.tcl,v 2.8 1998/09/09 17:50:23 abbottd Exp $}"
    
    class TS {
	inherit ROC

	method download        {runType}    {}
	method c_download      {runType}    @roc_download
	method prestart        {}           {}
	method c_prestart      {}           @roc_prestart
	method set_roc_mask    {runType}    {}

#	public variable config
	public variable rocMask  @rocMask
    }

    body TS::download {runType} {
	set config $runType
	c_download $runType
    }

    body TS::prestart {} {
	if {[catch "set_roc_mask ${config}" res]} {
          dalogmsg "ERROR" "set_roc_mask: $res"
        }
	c_prestart
    }
    
    body TS::set_roc_mask {runType} {
	if {![catch "database query \"SELECT value FROM ${runType}_option WHERE name='rocMask'\" " res]} {
#	    puts "set_roc_mask result is $res"
	    set res [database get next]
	    set rocMask $res
	    dalogmsg "INFO" "set ROC mask to 0x[format %08x $rocMask]" 
	}
	return
    }
    
}
