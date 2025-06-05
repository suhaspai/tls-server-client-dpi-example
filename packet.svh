// Credits: 
// https://www.chipverify.com/systemverilog/systemverilog-randomization
// https://stackoverflow.com/questions/65823427/randomizing-structure-with-typedefs

`ifndef BUFLEN
 `define BUFLEN 4
`endif

typedef struct packed {
   bit [31:0]  control;
   logic [`BUFLEN-1:0][31:0] buffer;
} packet_t;

class packet;

   rand packet_t data;

   // This is just a function to display current values of these variables
   function void display ();
      $write (" control= %08x", data.control);
      $write(" data[0-%2d]= ", `BUFLEN-1);
      for (int i=0; i<`BUFLEN; i++)
        $write(" %08x", data.buffer[i]);
   endfunction // display
   
endclass // packet
