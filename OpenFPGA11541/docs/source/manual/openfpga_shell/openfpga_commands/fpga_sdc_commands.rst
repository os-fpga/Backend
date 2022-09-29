.. _openfpga_sdc_commands:

FPGA-SDC
--------

write_pnr_sdc
~~~~~~~~~~~~~
 
  Write the SDC files for PnR backend
  
  .. option:: --file <string> or -f <string>
  
    Specify the output directory for SDC files
    For example, ``--file /temp/pnr_sdc``
  
  .. option:: --hierarchical 
  
    Output SDC files without full path in hierarchy
  
  .. option:: --flatten_names
  
     Use flatten names (no wildcards) in SDC files
  
  .. option:: --time_unit <string>
      
    Specify a time unit to be used in SDC files. Acceptable values are string: ``as`` | ``fs`` | ``ps`` | ``ns`` | ``us`` | ``ms`` | ``ks`` | ``Ms``. By default, we will consider second (``s``).
  
   
  .. option:: --output_hierarchy
  
    Output hierarchy of Multiple-Instance-Blocks(MIBs) to plain text file. This is applied to constrain timing for grids, Switch Blocks and Connection Blocks. 
  
    .. note:: Valid only when ``compress_routing`` is enabled in ``build_fabric``
  
  .. option:: --constrain_global_port
  
    Constrain all the global ports of FPGA fabric.
  
  .. option:: --constrain_non_clock_global_port
  
    Constrain all the non-clock global ports as clocks ports of FPGA fabric
  
    .. note:: ``constrain_global_port`` will treat these global ports in Clock Tree Synthesis (CTS), in purpose of balancing the delay to each sink. Be carefull to enable ``constrain_non_clock_global_port``, this may significanly increase the runtime of CTS as it is supposed to be routed before any other nets. This may cause routing congestion as well.
  
  .. option:: --constrain_grid
  
    Constrain all the grids of FPGA fabric
  
  .. option:: --constrain_sb
  
    Constrain all the switch blocks of FPGA fabric
  
  .. option:: --constrain_cb
  
    Constrain all the connection blocks of FPGA fabric
  
  .. option:: --constrain_configurable_memory_outputs
  
    Constrain all the outputs of configurable memories of FPGA fabric
  
  .. option:: --constrain_routing_multiplexer_outputs
  
    Constrain all the outputs of routing multiplexer of FPGA fabric
  
  .. option:: --constrain_switch_block_outputs
  
    Constrain all the outputs of switch blocks of FPGA fabric
  
  .. option:: --constrain_zero_delay_paths
  
    Constrain all the zero-delay paths in FPGA fabric
  
    .. note:: Zero-delay path may cause errors in some PnR tools as it is considered illegal
    
  .. option:: --verbose
  
    Enable verbose output
  
write_configuration_chain_sdc
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 
  Write the SDC file to constrain the timing for configuration chain. The timing constraints will always start from the first output (Q) of a Configuration Chain Flip-flop (CCFF) and ends at the inputs of the next CCFF in the chain. Note that Qb of CCFF will not be constrained!

  .. option:: --file <string> or -f <string> 
  
    Specify the output SDC file. For example, ``--file cc_chain.sdc``
  
  .. option:: --time_unit <string>
  
    Specify a time unit to be used in SDC files. Acceptable values are string: ``as`` | ``fs`` | ``ps`` | ``ns`` | ``us`` | ``ms`` | ``ks`` | ``Ms``. By default, we will consider second (``s``).
  
  .. option:: --max_delay <float>
  
    Specify the maximum delay to be used. The timing value should follow the time unit defined in this command.
  
  .. option:: --min_delay <float>
  
    Specify the minimum delay to be used. The timing value should follow the time unit defined in this command.
  
    .. note:: Only applicable when configuration chain is used as configuration protocol

write_sdc_disable_timing_configure_ports
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Write the SDC file to disable timing for configure ports of programmable modules. The SDC aims to break the combinational loops across FPGAs and avoid false path timing to be visible to timing analyzers

  .. option:: --file <string> or -f <string> 
  
    Specify the output SDC file. For example, ``--file disable_config_timing.sdc``.
  
  .. option:: --flatten_names
  
    Use flatten names (no wildcards) in SDC files
  
  .. option:: --verbose
  
    Show verbose log

write_analysis_sdc
~~~~~~~~~~~~~~~~~~

  Write the SDC to run timing analysis for a mapped FPGA fabric

  .. option:: --file <string> or -f <string>

    Specify the output directory for SDC files. For example, ``--file counter_sta_analysis.sdc``
  
  .. option:: --flatten_names

    Use flatten names (no wildcards) in SDC files

  .. option:: --time_unit <string>

    Specify a time unit to be used in SDC files. Acceptable values are string: ``as`` | ``fs`` | ``ps`` | ``ns`` | ``us`` | ``ms`` | ``ks`` | ``Ms``. By default, we will consider second (``s``).
