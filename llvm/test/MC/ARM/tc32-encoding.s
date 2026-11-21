@ RUN: llvm-mc -triple=thumbv6m-none-eabi -mcpu=arm9 -mattr=+tc32 -show-encoding %s \
@ RUN:   | FileCheck %s

@ TC32 permutes bits[15:12] of every 16-bit halfword (forward table). These
@ are the bytes the TLSR8266 executes; they differ from plain Thumb-1.
@ CHECK: push {r4, lr}                  @ encoding: [0x10,0x65]
@ CHECK: pop {r4, pc}                   @ encoding: [0x10,0x6d]
@ CHECK: rsbs r1, r2, #0                @ encoding: [0x51,0x02]
@ CHECK: tst r1, r2                     @ encoding: [0x11,0x02]
@ CHECK: adr r0, .Ltarget               @ encoding: [A,0x70]
@ CHECK: bl .Ltarget                    @ encoding: [A,0x90'A',A,0x98'A']
@ CHECK: mcsr r0                        @ encoding: [0xc0,0x6b]
@ CHECK: mrcs r1                        @ encoding: [0xc9,0x6b]
@ CHECK: mssr r2                        @ encoding: [0xd2,0x6b]
@ CHECK: mrss r3                        @ encoding: [0xdb,0x6b]
@ CHECK: reti {pc}                      @ encoding: [0x00,0x69]
@ CHECK: reti {r0, r1, r2, r3, r4, r5, r6, r7, pc} @ encoding: [0xff,0x69]
@ CHECK: mov r8, r8                     @ encoding: [0xc0,0x06]
        push {r4, lr}
        pop {r4, pc}
        rsbs r1, r2, #0
        tst r1, r2
        adr r0, .Ltarget
        bl .Ltarget
        mcsr r0
        mrcs r1
        mssr r2
        mrss r3
        reti {pc}
        reti {r0, r1, r2, r3, r4, r5, r6, r7, pc}
.Ltarget:
        nop
