// Both simulators use this explicitly inactive JTAG harness policy.
module SimJTAG #(parameter TICK_DELAY = 3)(
  input clock, input reset, input enable, input init_done,
  output jtag_TCK, output jtag_TMS, output jtag_TDI, output jtag_TRSTn,
  input jtag_TDO_data, input jtag_TDO_driven, output [31:0] exit
);
  assign jtag_TCK = 0;
  assign jtag_TMS = 1;
  assign jtag_TDI = 0;
  assign jtag_TRSTn = 0;
  assign exit = 0;
endmodule
