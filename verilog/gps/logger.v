//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

`default_nettype none

module LOGGER (
    input  wire clk,
    input  wire rst,
    input  wire rd,
    input  wire wr,
    input  wire [15:0] din,
    output wire [15:0] dout);
        
    reg [9:0] addra;
    reg [9:0] addrb;
    reg       full;
    
    RAMB16_S18_S18 logger_ram (
        .CLKA   (clk),          .CLKB   (clk),
        .DIPA   (),             .DIPB   (2'b11),
        .DOPA   (),             .DOPB   (),
        .SSRA   (1'b0),         .SSRB   (1'b0),
        .ENA    (1'b1),         .ENB    (wr && ~full),
        .DOA    (dout),         .DOB    (),
        .ADDRA  (addra + rd),   .ADDRB  (addrb),
        .DIA    (),             .DIB    (din),
        .WEA    (1'b0),         .WEB    (1'b1));

    always @ (posedge clk)
        if (rst)
            {addra, addrb, full} <= 0;
        else begin
            if (wr && ~full) {full, addrb} <= addrb + 1;
            addra <= addra + rd;
         end
         
endmodule
