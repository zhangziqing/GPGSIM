circuit RamHelperProbe :
  extmodule RAMHelper :
    input clk : Clock
    input en : UInt<1>
    input rIdx : UInt<64>
    output rdata : UInt<64>
    input wIdx : UInt<64>
    input wdata : UInt<64>
    input wmask : UInt<64>
    input wen : UInt<1>
    defname = RAMHelper
  module RamHelperProbe :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<64>
    reg count : UInt<16>, clock with :
      reset => (reset, UInt<16>(0))
    count <= add(count, UInt<16>(1))
    inst ram of RAMHelper
    ram.clk <= clock
    ram.en <= not(reset)
    ram.rIdx <= bits(sub(count, UInt<16>(1)), 3, 0)
    ram.wIdx <= bits(count, 3, 0)
    ram.wdata <= xor(UInt<64>("h0123456789abcdef"), pad(count, 64))
    ram.wmask <= mux(bits(count, 4, 4), UInt<64>("hff00ff00ff00ff00"), UInt<64>("h00ff00ff00ff00ff"))
    ram.wen <= UInt<1>(1)
    io <= ram.rdata
