circuit FlashHelperProbe :
  extmodule FlashHelper :
    input clk : Clock
    input ren : UInt<1>
    output data : UInt<64>
    input addr : UInt<32>
    defname = FlashHelper
  module FlashHelperProbe :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<64>
    reg count : UInt<16>, clock with :
      reset => (reset, UInt<16>(0))
    count <= add(count, UInt<16>(1))
    inst flash of FlashHelper
    flash.clk <= clock
    flash.ren <= not(bits(count, 2, 2))
    flash.addr <= bits(count, 4, 0)
    io <= flash.data
