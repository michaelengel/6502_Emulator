// Code examples for a simple 6502 CPU subset emulator
//
// Developed for the emulation tutorial at the 
// C64 Symposium in Bonn on July 8th, 2024
// http://rtro.de/c64
//
// Michael Engel (engel@multicores.org)
//
// Public domain code

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

// Variables to hold the processor and memory state
uint16_t pc;               // Program Counter
uint8_t  a, x, y, s;       // Accumulator, X and Y index, Stack pointer
uint8_t  p;                // Processor flags
uint8_t  m[65536] = { 0 }; // 64 kB main memory

enum { CF=0,ZF,IF,DF,BF,XX,VF,NF };       // Flag bit positions in P register

// Helper functions to access memory
// r(ead) or w(rite) an 8 or 16 bit (little endian) quantity
uint8_t r8(uint16_t addr) {
  return m[addr];
}

uint16_t r16(uint16_t addr) {
  return m[addr] + ((uint16_t)m[addr+1] << 8);
}

// Special case in w8:

// We emulate a single memory-mapped I/O device
// at address 0xC000

// Writing to 0xC000 outputs the written ASCII 
// character to the console
void w8(uint16_t addr, uint8_t val) {
  if (addr == 0xc000) {
    putchar(val);
  } else { 
    m[addr] = val;
  }
}

void w16(uint16_t addr, uint16_t val) {
  m[addr] = val & 0xFF;
  m[addr + 1] = val >> 8;
}

// Helper functions to set, reset and query the 
// processor flags (not all implemented!)
void set_z(void) {
  p = p | (1 << ZF);
}

void clr_z(void) {
  p = p & ~(1 << ZF);
}

void set_n(void) {
  p = p | (1 << NF);
}

void clr_n(void) {
  p = p & ~(1 << NF);
}

int get_c(void) {
  if ((p & (1 << CF)) != 0) {
    return 1;
  } else { 
    return 0;
  } 
}

void set_c(void) {
  p = p | (1 << CF);
}

void clr_c(void) {
  //    p = 01001101 (C = 1)
  //    AND 11111110 
  // -> p = 01001100 (C = 0)
  p = p & ~(1 << CF);
}

int main(void) {
  // Open the binary file with the object code
  // We currently load the code starting at address 0
  int fd = open("o6502.bin", O_RDONLY);
  if (fd < 0) { perror("open"); exit(1); } // Check problems

  // Read the binary file into the memory array
  read(fd, m, 65536);

  // Close access to the binary file
  close(fd);

  // Set reset vector to start executing code at 0 (little end)
  // The reset vector is located at addresses 0xFFFC/0xFFFD in the 6502
  // (little endian byte order)
  // The first instruction will be fetched from here

  // NOTE: In a more complex emulator, we would load a ROM
  // (e.g. the C64 KERNAL) to the highest memory location,
  // this in turn contains the reset vector and the code 
  // which the CPU then jumps to
  w16(0xFFFC, 0);

  // Get the address of the first instruction
  pc = r16(0xFFFC);

  // Reset the registers to a default value
  a = 0; x = 0; y = 0; s = 0; p = 0;

  // Our emulation loop (only ends when a BRK instruction is executed)
  while (1) {
    uint8_t opcode = r8(pc);  // Get the opcode at the current PC
    uint8_t param8;           // Variables for 8 and 16 bit parameters
    uint16_t param16;

    // Tracing
    fprintf(stderr, "PC: %04x opcode = %02x ", pc, opcode);

    // Which opcode is it?
    // We currently only emulate 8 of the 151 opcodes:
    // NOP, BRK, CLC    (implied addressing mode)
    // LDA#, ADC#, CMP# (immediate addr. mode, parameter in byte after opcode)
    // STA              (absolute addr. mode, param in 16 bits after opcode)
    // BCC              (relative addr. mode, offset in 8 bits after opcode)
    switch(opcode) {
      case 0x00: // BRK - exits the emulator (different to real hardware)
        exit(0);
        break;
      case 0xea: // NOP - does nothing
        pc = pc + 1;
        break;
      case 0x18: // CLC - clears the carry flag
        clr_c();
        pc = pc + 1;
        break;
      case 0xA9: // LDA #xx - loads value xx into A and (re)sets Z and N flags
        param8 = r8(pc+1);
        a = param8;
        if (a == 0) set_z(); else clr_z(); 
        if ((a & 0x80) != 0) set_n(); else clr_n(); 
        pc = pc + 2;
        break;
      case 0x8D: // STA xxxx - stores value in A into memory at address xxxx
        param16 = r16(pc+1);
        w8(param16, a);
        pc = pc + 3;
        break;
      case 0x69: // ADC #xx - adds xx plus the value of the C flag to A
                 // (re)sets Z and N flags accordingly
                 // NOTE: C and V are not calculated right now
        param8 = r8(pc+1);
        a = a + param8 + get_c();
        if (a == 0) set_z(); else clr_z(); 
        if ((a & 0x80) != 0) set_n(); else clr_n(); 
        pc = pc + 2;
        break;
      case 0xC9: // CMP #xx - compares A to xx (subtract xx from A)
                 // (re)sets C and Z
                 // NOTE: remaining flags are not calculated right now 
        param8 = r8(pc+1);
        if (a == param8) { set_z(); set_c(); }
        if (a < param8)  { clr_z(); clr_c(); }
        if (a > param8)  { clr_z(); set_c(); }
        pc = pc + 2;
        break;
      case 0x90: // BCC rel - branches to (PC+2)+rel if C flag is cleared
                 // otherwise continues execution at address PC+2
        param8 = r8(pc+1);
        if (get_c() == 1) { 
          pc = pc + 2;
        } else {
          // offset is a 2s complement number currently in an unsigned variable
          // typecast the bit pattern in param8 to a _signed_ 8 bit integer
          // to convince the compiler that the value is to be interpreted
          // as a 2s complement number (range -128...+127) 
          int8_t offset = (int8_t)param8;
          pc = pc + 2 + offset;
        }
        break;
      default: // not implemented and illegal opcodes terminate the emulator
        printf("Unknown opcode %02x at address %04x\n", opcode, pc);
        exit(1);
        break;
    }

    // second part of logging to output the updated values of the 
    // registers after executing the current instruction
    fprintf(stderr, "a: %02x x: %02x y: %02x s: 01%02x p: %02x\n", a, x, y, s, p);
  }
}
