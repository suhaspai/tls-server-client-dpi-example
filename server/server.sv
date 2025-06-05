// Server DPI Module
/* verilator lint_off WIDTHEXPAND */
/* verilator lint_off WIDTHTRUNC */

`define transport "tcp"
`include "packet.svh"

// input/output is with respect to C DPI function
import "DPI-C" function int net_open_server_socket(input string transport);
import "DPI-C" function int server_send(input  packet_t tx_pkt);
import "DPI-C" function int server_recv(output packet_t rx_pkt);
import "DPI-C" function void close_socket();

module server;
   timeunit 1ns;
   timeprecision 1fs;

   int ret;
   bit xfer_clk;

   bit verbose     = 0;
   int delay       = 1;
   int counter     = 0;
   int val;
   
   logic [2:0]  rate;

   // Create a class object handle   
   packet    rx_pkt;
   packet    tx_pkt;

   initial begin
      $printtimescale(server);

      // Instantiate the object, and allocate memory to this variable
      rx_pkt = new();
      tx_pkt = new();
     
      // handle all user inputs
      if ($test$plusargs("verbose")) begin
         verbose = 1;
         $display("%m verbosity level is set to true");
      end
      
      if ($value$plusargs("delay=%d", val)) begin
         delay=val;
         $display("%m delay is set to %d", delay);
      end

      // open socket communication 
      ret = net_open_server_socket(`transport);
      if (ret != 0)
        begin
           $display("Could not connect to server");
           $finish(1);
        end

      xfer_clk = 0;
      rate     = 0;
      forever begin
         casez(rate)
      	   3'h0:    #200ps    xfer_clk = ~xfer_clk;  // Gen1 2.5 GHz
      	   3'h1:    #100ps    xfer_clk = ~xfer_clk;  // Gen2 5 GHz
      	   3'h2:    #62.5ps   xfer_clk = ~xfer_clk;  // Gen3 8 GHz
      	   3'h3:    #31.25ps  xfer_clk = ~xfer_clk;  // Gen4 16 GHz
      	   3'h4:    #15.625ps xfer_clk = ~xfer_clk;  // Gen5 32 GHz (NRZ: 1b/UI)
           3'h5:    #15.625ps xfer_clk = ~xfer_clk;  // Gen6 32 Ghz (PAM4:2b/UI)
      	   default: #200ps    xfer_clk = ~xfer_clk;  // default Gen1
         endcase

         counter++;
         
         if (counter == `BUFLEN) begin
            ret = tx_pkt.randomize();
            assert(ret != 0);
            ret = server_send(tx_pkt.data);
            if (ret <= 0) begin
               close_socket();
               $finish(1);
            end
            else if (verbose) begin
               $write("%m TX:");
               tx_pkt.display();
               $display (" rate= %d", rate);
            end

            ret = server_recv(rx_pkt.data);
            if (ret <= 0) begin
               close_socket();
               $finish(1);
            end else if (verbose) begin
               $write("%m RX:");
               rx_pkt.display();
               $display (" rate= %d", rate);
            end

            counter = 0;

         end // if (counter == `BUFLEN)

      end // forever begin

   end // initial begin
   

   initial begin

      #(delay) rate = 'h1;
      #(delay) rate = 'h2;
      #(delay) rate = 'h3;
      #(delay) rate = 'h4;
      #(delay) rate = 'h5;      
      #(delay)

      #0 close_socket();

      $timeformat(-9, 0, "ns", 5);
      $display("%m sim_time= %t", $realtime);
      $finish(1);
   end

endmodule // server


