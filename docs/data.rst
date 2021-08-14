*****
Standard Data
*****

CPU Architectures
*****
Accessible as cpu.architecture
- cortex_m0
- cortex_m3
- cortex_m4
- cortex_a9
- cortex_a78
- x86
- x64
- z80
- psoc
- xtensa
- pic24

.. From Clang
- arm            .. ARM (little endian): arm, armv.*, xscale
- armeb          .. ARM (big endian): armeb
- aarch64        .. AArch64 (little endian): aarch64
- aarch64_be     .. AArch64 (big endian): aarch64_be
- aarch64_32     .. AArch64 (little endian) ILP32: aarch64_32
- arc            .. ARC: Synopsys ARC
- avr            .. AVR: Atmel AVR microcontroller
- bpfel          .. eBPF or extended BPF or 64-bit BPF (little endian)
- bpfeb          .. eBPF or extended BPF or 64-bit BPF (big endian)
- csky           .. CSKY: csky
- hexagon        .. Hexagon: hexagon
- m68k           .. M68k: Motorola 680x0 family
- mips           .. MIPS: mips, mipsallegrex, mipsr6
- mipsel         .. MIPSEL: mipsel, mipsallegrexe, mipsr6el
- mips64         .. MIPS64: mips64, mips64r6, mipsn32, mipsn32r6
- mips64el       .. MIPS64EL: mips64el, mips64r6el, mipsn32el, mipsn32r6el
- msp430         .. MSP430: msp430
- ppc            .. PPC: powerpc
- ppcle          .. PPCLE: powerpc (little endian)
- ppc64          .. PPC64: powerpc64, ppu
- ppc64le        .. PPC64LE: powerpc64le
- r600           .. R600: AMD GPUs HD2XXX - HD6XXX
- amdgcn         .. AMDGCN: AMD GCN GPUs
- riscv32        .. RISC-V (32-bit): riscv32
- riscv64        .. RISC-V (64-bit): riscv64
- sparc          .. Sparc: sparc
- sparcv9        .. Sparcv9: Sparcv9
- sparcel        .. Sparc: (endianness = little). NB: 'Sparcle' is a CPU variant
- systemz        .. SystemZ: s390x
- tce            .. TCE (http:..tce.cs.tut.fi/): tce
- tcele          .. TCE little endian (http:..tce.cs.tut.fi/): tcele
- thumb          .. Thumb (little endian): thumb, thumbv.*
- thumbeb        .. Thumb (big endian): thumbeb
- x86            .. X86: i[3-9]86
- x86_64         .. X86-64: amd64, x86_64
- xcore          .. XCore: xcore
- nvptx          .. NVPTX: 32-bit
- nvptx64        .. NVPTX: 64-bit
- le32           .. le32: generic little-endian 32-bit CPU (PNaCl)
- le64           .. le64: generic little-endian 64-bit CPU (PNaCl)
- amdil          .. AMDIL
- amdil64        .. AMDIL with 64-bit pointers
- hsail          .. AMD HSAIL
- hsail64        .. AMD HSAIL with 64-bit pointers
- spir           .. SPIR: standard portable IR for OpenCL 32-bit version
- spir64         .. SPIR: standard portable IR for OpenCL 64-bit version
- kalimba        .. Kalimba: generic kalimba
- shave          .. SHAVE: Movidius vector VLIW processors
- lanai          .. Lanai: Lanai 32-bit
- wasm32         .. WebAssembly with 32-bit pointers
- wasm64         .. WebAssembly with 64-bit pointers
- renderscript32 .. 32-bit RenderScript
- renderscript64 .. 64-bit RenderScript
- ve             .. NEC SX-Aurora Vector Engine

Accessible as cpu.endian
- little_endian
- big_endian

Compilers
*****
- gcc
- clang
- iar
- keil
- armcc
- msvc
- tasking

