circuit ActiveFalling :
  module ActiveFalling :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<8>
    node falling = asClock(eq(asUInt(clock), UInt<1>(0)))
    reg counter : UInt<8>, falling with :
      reset => (reset, UInt<8>(0))
    counter <= add(counter, UInt<8>(1))
    io <= counter
