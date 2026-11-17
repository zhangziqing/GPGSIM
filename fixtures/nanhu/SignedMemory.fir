circuit GatedMemory :
  module GatedMemory :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<8>
    reg count : UInt<6>, clock with :
      reset => (reset, UInt<6>(0))
    count <= add(count, UInt<6>(1))
    mem ram :
      data-type => SInt<6>
      depth => 16
      reader => r
      writer => w
      read-latency => 1
      write-latency => 1
      read-under-write => old
    ram.r.clk <= clock
    ram.r.en <= UInt<1>(1)
    ram.r.addr <= bits(sub(count, UInt<6>(1)), 3, 0)
    ram.w.clk <= clock
    ram.w.en <= not(reset)
    ram.w.mask <= UInt<1>(1)
    ram.w.addr <= bits(count, 3, 0)
    ram.w.data <= asSInt(count)
    io <= asUInt(pad(ram.r.data, 8))
