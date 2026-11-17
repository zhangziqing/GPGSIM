module STD_CLKGT_func(input CK, input E, input TE, output Q);
  reg enable_latch;
  always_latch if (!CK) enable_latch = E | TE;
  assign Q = CK & enable_latch;
endmodule
