# Raspberry Pi ↔ FPGA Ethernet Control

A hardware/software co-design project implementing direct Ethernet
communication between a Raspberry Pi 5 and a Digilent Arty A7 FPGA.

The Raspberry Pi sends a decimal value from 0–15 over TCP. An lwIP
TCP server running on a MicroBlaze soft processor receives the command
and drives a 4-bit AXI GPIO peripheral, displaying the corresponding
binary value on the Arty A7 LEDs.

## Architecture

Raspberry Pi 5
    ↓ TCP / 100BASE-T
AXI EthernetLite
    ↓
lwIP
    ↓
MicroBlaze
    ↓
AXI GPIO
    ↓
Arty A7 LEDs