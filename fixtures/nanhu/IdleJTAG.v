// Inactive debug harness policy used for the Nanhu benchmark.
module SimJTAG(output jtag_TCK, output jtag_TMS);
  assign jtag_TCK = 1'b0;
  assign jtag_TMS = 1'b1;
endmodule
