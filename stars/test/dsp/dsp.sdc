create_clock -period 7.935999999999999 -name clk  
set_input_delay 1 -clock clk [get_ports {in*}]  
set_output_delay 1 -clock clk [get_ports {z*}]  
