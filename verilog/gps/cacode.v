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

module CACODE (
    input  wire        rst,
    input  wire        clk,
    input  wire        g2_init,
    input  wire [10:1] init,
    input  wire        rd,
    
    output wire        chip
);
    wire [3:0] T0, T1;
    assign T0 = init[8:5];
    assign T1 = init[4:1];
    
	reg [10:1] g1, g2;
	
    always @ (posedge clk)
        if (rst) begin
            g1 <= 10'b1111111111;
            g2 <= g2_init? init : 10'b1111111111;
        end else 
            if (rd) begin
                g1[10:1] <= {g1[9:1], g1[3] ^ g1[10]};
                g2[10:1] <= {g2[9:1], g2[2] ^ g2[3] ^ g2[6] ^ g2[8] ^ g2[9] ^ g2[10]};
            end

    assign chip = g2_init? (g1[10] ^ g2[10]) : (g1[10] ^ g2[T0] ^ g2[T1]);

endmodule
