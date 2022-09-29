.. _run_fpga_task:

OpenFPGA Task
---------------

Tasks provide a framework for running the :ref:`run_fpga_flow` on
multiple benchmarks, architectures, and set of OpenFPGA parameters.
The structure of the framework is very similar to
`VTR-Tasks <https://docs.verilogtorouting.org/en/latest/vtr/tasks/>`_
implementation with additional functionality and minor file extension changes.

Task Directory
~~~~~~~~~~~~~~

The tasks are stored in a ``TASK_DIRECTORY``, which by default points to
``${OPENFPGA_PATH}/openfpga_flow/tasks``. Every directory or sub-directory in
task directory consisting of ``../config/task.conf`` file can be referred to as a
task.

To create as task name called ``basic_flow`` following directory has to exist::

   ${TASK_DIRECTORY}/basic_flow/conf/task.conf

Similarly  ``regression/regression_quick`` expect following structure::

   ${TASK_DIRECTORY}/regression/regression_quick/conf/task.conf


Running OpenFPGA Task:
~~~~~~~~~~~~~~~~~~~~~~

At a minimum ``open_fpga_flow.py`` requires following command-line arguments::

    open_fpga_flow.py <task1_name> <task2_name> ... [<options>]

where:

  * ``<task_name>`` is the name of the task to run
  * ``<options>`` Other command line arguments described below


Command-line Options
~~~~~~~~~~~~~~~~~~~~

.. option:: --maxthreads <number_of_threads>

    This option defines the number of threads to run while executing task.
    Each combination of architecture, benchmark and set of OpenFPGA Flow options
    runs in a individual thread.

.. option:: --skip_thread_logs

    Passsing this option skips printing logs from each OpenFPGA Flow script run.

.. option:: --exit_on_fail

    Passsing this option exits the OpenFPGA task script with returncode 1,
    if any threads fail to execute successfully. It is mainly used to while
    performing regression test.

.. option:: --test_run

    This option allows to debug OpenFPGA Task script
    by skiping actual execution of OpenFPGA flow .
    Passing this option prints the list of
    commnad generated to execute using OpenFPGA flow.

.. option:: --debug

    To enable detailed log printing.


Creating a new OpenFPGA Task
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Create the folder ``${TASK_DIRECTORY}/<task_name>``
- Create a file ``${TASK_DIRECTORY}/<task_name>/config/task.conf`` in it
- Configure the task as explained in :ref:`Configuring a new OpenFPGA Task`


Configuring a new OpenFPGA Task
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The task configuration file ``task.conf`` consists of ``GENERAL``,
``ARCHITECTURES``, ``BENCHMARKS``, ``SYNTHESIS_PARAM`` and
``SCRIPT_PARAM_<var_name>`` sections.
Declaring all the above sections are mandatory.

.. note::
    The configuration file supports all the OpenFPGA Variables refer
    :ref:`openfpga-variables` section to know more. Variable in the configuration
    file is declared as ``${PATH:<variable_name>}``

General Section
^^^^^^^^^^^^^^^

.. option:: fpga_flow=<yosys_vpr|vpr_blif|yosys>

    This option defines which OpenFPGA flow to run. By default ``yosys_vpr`` is executed.

.. option:: power_analysis=<true|false>

    Specifies whether to perform power analysis or not.

.. option:: power_tech_file=<path_to_tech_XML_file>

    Declares which tech XML file to use while performing Power Analysis.

.. option:: spice_output=<true|false>

    Setting up this variable generates Spice Netlist at the end of the flow.
    Equivalent of passing ``--vpr_fpga_spice`` command to :ref:`run_fpga_flow`

.. option:: verilog_output=<true|false>

    Setting up this variable generates Verilog Netlist at the end of the flow.
    Equivalent of passing ``--vpr_fpga_spice`` command to :ref:`run_fpga_flow`

.. option:: timeout_each_job=<true|false>

    Specifies the timeout for each :ref:`run_fpga_flow` execution. Default is set to ``20 min.``

.. option:: verific=<true|false>

    Specifies to use Verific as a frontend for Yosys while running a yosys_vpr flow.
    The following standards are used by default for reading input HDL files:
    * Verilog - ``vlog95``
    * System Verilog - ``sv2012``
    * VHDL - ``vhdl2008``
    The option should be used only with custom Yosys template containing Verific commands.


OpenFPGA_SHELL Sections
^^^^^^^^^^^^^^^^^^^^^^^

    User can specify OpenFPGA_SHELL options in this section.


Architectures Sections
^^^^^^^^^^^^^^^^^^^^^^

    User can define the list of architecture files in this section.

.. option:: arch<arch_label>=<xml_architecture_file_path>

    The ``arch_label`` variable can be any number of string without
    white-spaces. ``xml_architecture_file_path`` is path to the actual XML
    architecture file

.. note::

    In the final OpenFPGA Task result, the architecture will be referred by its
    ``arch_label``.

Benchmarks Sections
^^^^^^^^^^^^^^^^^^^

    User can define the list of benchmarks files in this section.

.. option:: bench<bench_label>=<list_of_files_in_benchmark>

    The ``bench_label`` variable can be any number of string without
    white-spaces. ``list_of_files_in_benchmark`` is a list of benchmark HDL files paths.

    For Example following code shows how to define a benchmarks,
    with a single file, multiple files and files added from a specific directory.

    .. code-block:: text

        [BENCHMARKS]
        # To declare single benchmark file
        bench_design1=${BENCH_PATH}/design/top.v

        # To declare multiple benchmark file
        bench_design2=${BENCH_PATH}/design/top.v,${BENCH_PATH}/design/sub_module.v

        # To add all files in specific directory to the benchmark
        bench_design3=${BENCH_PATH}/design/top.v,${BENCH_PATH}/design/lib/*.v

.. note::
    ``bench_label`` is referred again in ``Synthesis_Param`` section to
    provide additional information about benchmark

Synthesis Parameter Sections
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    User can define extra parameters for each benchmark in the
    ``BENCHMARKS`` sections.

.. option:: bench<bench_label>_top=<Top_Module_Name>

    This option defines the Top Level module name for ``bench_label`` benchmark.
    By default, the top-level module name is considered as a ``top``.

.. option:: bench<bench_label>_yosys=<yosys_template_file>

    This config defines Yosys template script file.

.. option:: bench<bench_label>_chan_width=<chan_width_to_use>

    In case of running fixed channel width routing for each benchmark,
    this option defines the channel width to be used for ``bench_label``
    benchmark

.. option:: bench<bench_label>_act=<activity_file_path>

    In case of running ``blif_vpr_flow`` this option provides the activity files
    to be used to generate testbench for ``bench_label`` benchmark

.. note:: This file is required only when the ``power_analysis`` option in the general section is enabled. Otherwise, it is optional

.. option:: bench<bench_label>_verilog=<source_verilog_file_path>

    In case of running ``blif_vpr_flow`` with verification this option provides
    the source Verilog design for ``bench_label`` benchmark to be used
    while verification.

.. option:: bench<bench_label>_read_verilog_options=<Options>

    This config defines the ``read_verilog`` command options for ``bench_label`` benchmark.

.. option:: bench<bench_label>_yosys_args=<Arguments>

    This config defines Yosys arguments to be used in QuickLogic synthesis script for ``bench_label`` benchmark.

.. option:: bench<bench_label>_yosys_dff_map_verilog=<dff_technology_file_path>

    This config defines DFF technology file to be used in technology mapping for ``bench_label`` benchmark. 

.. option:: bench<bench_label>_yosys_bram_map_verilog=<bram_technology_file_path>

    This config defines BRAM technology file to be used in technology mapping for ``bench_label`` benchmark. 

.. option:: bench<bench_label>_yosys_bram_map_rules=<bram_technology_rules_file_path>

    This config defines BRAM technology rules file to be used in technology mapping for ``bench_label`` benchmark.  This config should be used with ``bench<bench_label>_yosys_bram_map_verilog`` config.

.. option:: bench<bench_label>_yosys_dsp_map_verilog=<dsp_technology_file_path>

    This config defines DSP technology file to be used in technology mapping for ``bench_label`` benchmark. 

.. option:: bench<bench_label>_yosys_dsp_map_parameters=<dsp_mapping_parameters>

    This config defines DSP technology parameters to be used in technology mapping for ``bench_label`` benchmark.  This config should be used with ``bench<bench_label>_yosys_dsp_map_verilog`` config.

.. option:: bench<bench_label>_verific_include_dir=<include_dir_path>

    This config defines include directory path for ``bench_label`` benchmark. Verific will search in this directory to find included files. If there are multiple paths then they can be provided as a comma separated list.

.. option:: bench<bench_label>_verific_library_dir=<library_dir_path>

    This config defines library directory path for ``bench_label`` benchmark. Verific will search in this directory to find undefined modules. If there are multiple paths then they can be provided as a comma separated list.

.. option:: bench<bench_label>_verific_verilog_standard=<-vlog95|-vlog2k>

    The config specifies Verilog language standard to be used while reading the Verilog files for ``bench_label`` benchmark.

.. option:: bench<bench_label>_verific_systemverilog_standard=<-sv2005|-sv2009|-sv2012>

    The config specifies SystemVerilog language standard to be used while reading the SystemVerilog files for ``bench_label`` benchmark.

.. option:: bench<bench_label>_verific_vhdl_standard=<-vhdl87|-vhdl93|-vhdl2k|-vhdl2008>

    The config specifies VHDL language standard to be used while reading the VHDL files for ``bench_label`` benchmark.

.. option:: bench<bench_label>_verific_read_lib_name<lib_label>=<lib_name>

    The ``lib_label`` variable can be any number of string without white-spaces. The config specifies library name for ``bench_label`` benchmark where Verilog/SystemVerilog/VHDL files specified by ``bench<bench_label>_verific_read_lib_src<lib_label>`` config will be loaded. This config should be used only with ``bench<bench_label>_verific_read_lib_src<lib_label>`` config.

.. option:: bench<bench_label>_verific_read_lib_src<lib_label>=<library_src_files>

    The ``lib_label`` variable can be any number of string without white-spaces. The config specifies Verilog/SystemVerilog/VHDL files to be loaded into library specified by ``bench<bench_label>_verific_read_lib_name<lib_label>`` config for ``bench_label`` benchmark. The ``library_src_files`` should be the source files names separated by commas. This config should be used only with ``bench<bench_label>_verific_read_lib_name<lib_label>`` config.

.. option:: bench<bench_label>_verific_search_lib=<lib_name>

    The config specifies library name for ``bench_label`` benchmark from where Verific will look up for external definitions while reading HDL files.

.. option:: bench<bench_label>_yosys_cell_sim_verilog=<verilog_files>

    The config specifies Verilog files for ``bench_label`` benchmark which should be separated by comma.

.. option:: bench<bench_label>_yosys_cell_sim_systemverilog=<systemverilog_files>

    The config specifies SystemVerilog files for ``bench_label`` benchmark which should be separated by comma.

.. option:: bench<bench_label>_yosys_cell_sim_vhdl=<vhdl_files>

    The config specifies VHDL files for ``bench_label`` benchmark which should be separated by comma.

.. option:: bench<bench_label>_yosys_blackbox_modules=<blackbox_modules>

    The config specifies blackbox modules names for ``bench_label`` benchmark which should be separated by comma (usually these are the modules defined in files specified with bench<bench_label>_yosys_cell_sim_<verilog/systemverilog/vhdl> option).

.. note::
    The following configs might be common for all benchmarks:

* ``bench<bench_label>_yosys``
* ``bench<bench_label>_chan_width``
* ``bench<bench_label>_read_verilog_options``
* ``bench<bench_label>_yosys_args``
* ``bench<bench_label>_yosys_bram_map_rules``
* ``bench<bench_label>_yosys_bram_map_verilog``
* ``bench<bench_label>_yosys_cell_sim_verilog``
* ``bench<bench_label>_yosys_cell_sim_systemverilog``
* ``bench<bench_label>_yosys_cell_sim_vhdl``
* ``bench<bench_label>_yosys_blackbox_modules``
* ``bench<bench_label>_yosys_dff_map_verilog``
* ``bench<bench_label>_yosys_dsp_map_parameters``
* ``bench<bench_label>_yosys_dsp_map_verilog``
* ``bench<bench_label>_verific_verilog_standard``
* ``bench<bench_label>_verific_systemverilog_standard``
* ``bench<bench_label>_verific_vhdl_standard``
* ``bench<bench_label>_verific_include_dir``
* ``bench<bench_label>_verific_library_dir``
* ``bench<bench_label>_verific_search_lib``

*The following syntax should be used to define common config:* ``bench_<config_name>_common``

Script Parameter Sections
^^^^^^^^^^^^^^^^^^^^^^^^^
The script parameter section lists set of commnad line pararmeters to be passed to :ref:`run_fpga_flow` script. The section name is defines as ``SCRIPT_PARAM_<parameter_set_label>`` where `parameter_set_label` can be any word without white spaces.
The section is referred with ``parameter_set_label`` in the final result file.

For example following code Specifies the two sets (``Fixed_Routing_30`` and ``Fixed_Routing_50``) of :ref:`run_fpga_flow` arguments.

.. code-block:: text

    [SCRIPT_PARAM_Fixed_Routing_30]
    # Execute fixed routing with channel with 30
    fix_route_chan_width=30

    [SCRIPT_PARAM_Fixed_Routing_50]
    # Execute fixed routing with channel with 50
    fix_route_chan_width=50

Example Task Configuration File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. code-block:: text

    [GENERAL]
    spice_output=false
    verilog_output=false
    power_analysis = true
    power_tech_file = ${PATH:TECH_PATH}/winbond90nm/winbond90nm_power_properties.xml
    timeout_each_job = 20*60

    [ARCHITECTURES]
    arch0=${PATH:ARCH_PATH}/winbond90/k6_N10_rram_memory_bank_SC_winbond90.xml

    [BENCHMARKS]
    bench0=${PATH:BENCH_PATH}/MCNC_Verilog/s298/s298.v
    bench1=${PATH:BENCH_PATH}/MCNC_Verilog/elliptic/elliptic.v

    [SYNTHESIS_PARAM]
    bench0_top = s298
    bench1_top = elliptic

    [SCRIPT_PARAM_Slack_30]
    min_route_chan_width=1.3

    [SCRIPT_PARAM_Slack_80]
    min_route_chan_width=1.8
