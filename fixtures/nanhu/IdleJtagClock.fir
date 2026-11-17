circuit GatedMemory :
  extmodule SimJTAG :
    output jtag_TCK : Clock
    output jtag_TMS : UInt<1>
    defname = SimJTAG
  module IdleBank :
    input clock : Clock
    input reset : AsyncReset
    output io : UInt<8>
    node falling = asClock(eq(asUInt(clock), UInt<1>(0)))
    reg pos : UInt<8>, clock with :
      reset => (reset, UInt<8>(7))
    reg neg : UInt<8>, falling with :
      reset => (reset, UInt<8>(11))
    pos <= add(pos, UInt<8>(1))
    neg <= add(neg, UInt<8>(1))
    io <= add(pos, neg)
  module IdleBridge :
    input clock : UInt<1>
    input reset : AsyncReset
    output io : UInt<8>
    inst bank of IdleBank
    bank.clock <= asClock(clock)
    bank.reset <= reset
    io <= bank.io
  module GatedMemory :
    input clock : Clock
    input reset : UInt<1>
    output io : UInt<8>
    inst jtag of SimJTAG
    inst bridge of IdleBridge
    bridge.clock <= asUInt(jtag.jtag_TCK)
    bridge.reset <= asAsyncReset(reset)
    io <= bridge.io
