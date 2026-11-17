circuit GatedMemory :
  extmodule STD_CLKGT_func :
    input CK : Clock
    input E : UInt<1>
    input TE : UInt<1>
    output Q : Clock
    defname = STD_CLKGT_func
  module Bank :
    input clock : UInt<1>
    input reset : UInt<1>
    input data : UInt<8>
    output io : UInt<8>
    node local_clock = asClock(clock)
    reg count : UInt<4>, local_clock with :
      reset => (reset, UInt<4>(0))
    count <= add(count, UInt<4>(1))
    mem ram :
      data-type => UInt<8>
      depth => 16
      reader => r
      writer => w
      read-latency => 1
      write-latency => 1
      read-under-write => old
    ram.r.clk <= local_clock
    ram.r.en <= UInt<1>(1)
    ram.r.addr <= sub(count, UInt<4>(1))
    ram.w.clk <= local_clock
    ram.w.en <= not(reset)
    ram.w.mask <= UInt<1>(1)
    ram.w.addr <= count
    ram.w.data <= data
    io <= ram.r.data
  module BoolBridge :
    input clock : UInt<1>
    input reset : UInt<1>
    input data : UInt<8>
    output io : UInt<8>
    inst bank of Bank
    bank.clock <= clock
    bank.reset <= reset
    bank.data <= data
    io <= bank.io
  module GatedMemory :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<8>
    reg divider : UInt<1>, clock with :
      reset => (reset, UInt<1>(0))
    reg data : UInt<8>, clock with :
      reset => (reset, UInt<8>(0))
    divider <= not(divider)
    data <= add(data, UInt<8>(1))
    inst gate of STD_CLKGT_func
    gate.CK <= clock
    gate.E <= divider
    gate.TE <= UInt<1>(0)
    inst bank of BoolBridge
    node clock_as_bool = asUInt(gate.Q)
    bank.clock <= clock_as_bool
    bank.reset <= reset
    bank.data <= data
    io <= bank.io
