// -----------------------------------------------------------------------------
// Altair 8800 Simulator
// Copyright (C) 2017 David Hansel
// -----------------------------------------------------------------------------

#ifndef DAZZLER_H
#define DAZZLER_H

#include <Arduino.h>

extern uint16_t dazzler_mem_addr1, dazzler_mem_addr2, dazzler_mem_start, dazzler_mem_end;

#define dazzler_write_mem(a, v) { if( a<dazzler_mem_end && a>=dazzler_mem_start && Mem[a]!=v ) dazzler_write_mem_do(a, v); }

void dazzler_write_mem_do(uint16_t a, byte v);
void dazzler_out_ctrl(byte v);
void dazzler_out_pict(byte v);
void dazzler_out_dac(byte dacnum, byte v);
byte dazzler_in(byte port);

void dazzler_receive(int size, byte *data);

void dazzler_set_iface(byte iface);
byte dazzler_get_iface();
void dazzler_setup();

#define BUFFER1       0x00
#define BUFFER2       0x08

#endif
