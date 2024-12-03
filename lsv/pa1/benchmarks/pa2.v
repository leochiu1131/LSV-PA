module fuck(in0, in1, out);
input in0;
input in1;
output out;

wire not_in0;
wire not_in1;
wire not_temp0;
wire not_temp1;
wire not_temp2;
wire temp0;
wire temp1;
wire temp2;
wire temp3;
wire temp4;




not (not_in0, in0);
not (not_in1, in1);
and (temp0, in0, in1);
not (not_temp0, temp0);
and (temp1, not_in0, not_in1);
not (not_temp1, temp1);
and (temp2, not_temp0, not_temp1);
not (not_temp2, temp2);
and (temp3, temp0, not_temp1);
and (temp4, not_temp2, temp3);
buf (out, temp4);

endmodule