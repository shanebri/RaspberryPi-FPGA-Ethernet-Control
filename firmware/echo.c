/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

 #include <stdio.h>
 #include <string.h>
 
 #include "xgpio.h"
 #include "xparameters.h"
 #include "xil_printf.h"
 
 #include "lwip/err.h"
 #include "lwip/tcp.h"
 #include "lwip/pbuf.h"
 
 
 /* -------------------------------------------------------------------------
  * AXI GPIO
  * ------------------------------------------------------------------------- */
 
 static XGpio gpio;
 
 
 /*
  * Initialize the 4-bit AXI GPIO connected to the Arty A7 LEDs.
  */
 static int init_led_gpio(void)
 {
     XGpio_Config *cfg;
 
     xil_printf("Initializing LED GPIO...\r\n");
 
     /*
      * Vitis 2025.2 SDT flow uses the peripheral base address for
      * XGpio_LookupConfig().
      */
     cfg = XGpio_LookupConfig(XPAR_AXI_GPIO_0_BASEADDR);
 
     if (cfg == NULL) {
         xil_printf("ERROR: GPIO config lookup failed\r\n");
         return -1;
     }
 
     if (XGpio_CfgInitialize(&gpio,
                             cfg,
                             cfg->BaseAddress) != XST_SUCCESS) {
         xil_printf("ERROR: GPIO initialization failed\r\n");
         return -1;
     }
 
     /*
      * Channel 1:
      * 0 bit in TRI = output
      *
      * All four GPIO bits are outputs.
      */
     XGpio_SetDataDirection(&gpio, 1, 0x0);
 
     /*
      * Start with all LEDs off.
      */
     XGpio_DiscreteWrite(&gpio, 1, 0x0);
 
     xil_printf("LED GPIO ready\r\n");
 
     return 0;
 }
 
 
 /* -------------------------------------------------------------------------
  * TCP command parsing
  * ------------------------------------------------------------------------- */
 
 /*
  * Expected TCP payloads:
  *
  * "0\n"
  * "1\n"
  * "7\n"
  * "10\n"
  * "15\n"
  *
  * Returns:
  *      0  = valid
  *     -1  = invalid
  */
 static int parse_led_value(struct pbuf *p, u32 *value)
 {
     char buf[8];
     u16_t len;
     u16_t i;
     int v = 0;
     int digits = 0;
 
     if ((p == NULL) || (value == NULL)) {
         return -1;
     }
 
     /*
      * Use tot_len so this also works if lwIP gives us a chained pbuf.
      */
     len = p->tot_len;
 
     if (len >= sizeof(buf)) {
         len = sizeof(buf) - 1;
     }
 
     pbuf_copy_partial(p, buf, len, 0);
 
     buf[len] = '\0';
 
     for (i = 0; i < len; i++) {
 
         if ((buf[i] >= '0') && (buf[i] <= '9')) {
 
             v = (v * 10) + (buf[i] - '0');
             digits++;
 
         } else if ((buf[i] == '\n') || (buf[i] == '\r')) {
 
             break;
 
         } else {
 
             return -1;
         }
     }
 
     if (digits == 0) {
         return -1;
     }
 
     /*
      * Four LEDs = four bits = values 0 through 15.
      */
     if (v > 15) {
         return -1;
     }
 
     *value = (u32)v;
 
     return 0;
 }
 
 
 /* -------------------------------------------------------------------------
  * lwIP application functions
  * ------------------------------------------------------------------------- */
 
 int transfer_data()
 {
     return 0;
 }
 
 
 void print_app_header()
 {
 #if (LWIP_IPV6 == 0)
     xil_printf("\n\r\n\r----- Pi -> FPGA LED TCP Server -----\n\r");
 #else
     xil_printf("\n\r\n\r----- Pi -> FPGA LED TCP Server IPv6 -----\n\r");
 #endif
 
     xil_printf("Send a decimal number 0-15 to TCP port 7\r\n");
 }
 
 
 /* -------------------------------------------------------------------------
  * TCP receive callback
  * ------------------------------------------------------------------------- */
 
 err_t recv_callback(void *arg,
                     struct tcp_pcb *tpcb,
                     struct pbuf *p,
                     err_t err)
 {
     u32 led_value;
 
     /*
      * p == NULL means the remote side closed the connection.
      */
     if (!p) {
         tcp_close(tpcb);
         tcp_recv(tpcb, NULL);
 
         return ERR_OK;
     }
 
     /*
      * Tell lwIP that we consumed the received bytes.
      */
     tcp_recved(tpcb, p->tot_len);
 
 
     /*
      * Try to interpret the incoming TCP payload as an LED value.
      */
     if (parse_led_value(p, &led_value) == 0) {
 
         /*
          * Only the low four bits are physically connected to the LEDs.
          */
         XGpio_DiscreteWrite(&gpio, 1, led_value & 0xF);
 
         xil_printf("LED value = %d  binary bits = 0x%X\r\n",
                    (int)led_value,
                    (unsigned int)(led_value & 0xF));
 
     } else {
 
         xil_printf("Invalid LED command. Send a number from 0 to 15.\r\n");
     }
 
 
     /*
      * Preserve the original echo-server behavior.
      *
      * The Raspberry Pi will receive back whatever it sent.
      */
     if (tcp_sndbuf(tpcb) > p->len) {
 
         err = tcp_write(tpcb,
                         p->payload,
                         p->len,
                         TCP_WRITE_FLAG_COPY);
 
         if (err != ERR_OK) {
             xil_printf("tcp_write error: %d\r\n", err);
         }
 
     } else {
 
         xil_printf("No space in tcp_sndbuf\r\n");
     }
 
 
     /*
      * Free the packet after we're finished using it.
      */
     pbuf_free(p);
 
     return ERR_OK;
 }
 
 
 /* -------------------------------------------------------------------------
  * TCP connection callback
  * ------------------------------------------------------------------------- */
 
 err_t accept_callback(void *arg,
                       struct tcp_pcb *newpcb,
                       err_t err)
 {
     static int connection = 1;
 
     xil_printf("TCP client connected\r\n");
 
     /*
      * Install our receive callback for this TCP connection.
      */
     tcp_recv(newpcb, recv_callback);
 
 
     /*
      * Give this connection a simple integer ID.
      */
     tcp_arg(newpcb, (void *)(UINTPTR)connection);
 
     connection++;
 
     return ERR_OK;
 }
 
 
 /* -------------------------------------------------------------------------
  * Application startup
  * ------------------------------------------------------------------------- */
 
 int start_application()
 {
     struct tcp_pcb *pcb;
     err_t err;
     unsigned port = 7;
 
 
     /*
      * Initialize the AXI GPIO before starting the TCP server.
      */
     if (init_led_gpio() != 0) {
 
         xil_printf("ERROR: LED initialization failed\r\n");
 
         return -1;
     }
 
 
     /*
      * Create a new TCP protocol control block.
      */
     pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
 
     if (!pcb) {
 
         xil_printf("Error creating PCB. Out of Memory\r\n");
 
         return -1;
     }
 
 
     /*
      * Bind server to TCP port 7.
      */
     err = tcp_bind(pcb,
                    IP_ANY_TYPE,
                    port);
 
     if (err != ERR_OK) {
 
         xil_printf("Unable to bind to port %d: err = %d\r\n",
                    port,
                    err);
 
         tcp_close(pcb);
 
         return -2;
     }
 
 
     /*
      * No callback argument is needed for the listening PCB.
      */
     tcp_arg(pcb, NULL);
 
 
     /*
      * Put TCP PCB into LISTEN state.
      */
     pcb = tcp_listen(pcb);
 
     if (!pcb) {
 
         xil_printf("Out of memory while tcp_listen\r\n");
 
         return -3;
     }
 
 
     /*
      * Register callback for incoming TCP connections.
      */
     tcp_accept(pcb, accept_callback);
 
 
     xil_printf("TCP LED server started @ port %d\r\n", port);
 
     return 0;
 }