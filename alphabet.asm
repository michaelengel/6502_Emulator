start:
  CLC         // clear carry flag
  LDA #0x41   // first value in A: 0x41 = 'A'
loop:
  STA 0xC000  // output to screen
  ADC #0x01   // increment value in A: next character
  CMP #0x5B   // last character reached?
  BCC loop    // no? execute next loop iteration
  BRK         // that’s all
