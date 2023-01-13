create_clock -period 2.5 -name clk  
set_input_delay 1 -clock clk [get_ports in*]  
set_output_delay 1 -clock clk [get_ports out*]  
