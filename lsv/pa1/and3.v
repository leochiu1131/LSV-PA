module and3(in1,o);

input [2:0]in1;
output o;

assign o = in1[0] & in1[1] & in1[2];




endmodule
