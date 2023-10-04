module top (
    A, 
    B,
    Y
);
input [1:0] A;
input [1:0] B;
output [3:0] Y;
wire [3:0] Y;
wire [1:0] A;
wire [1:0] B;

assign Y[0] = A[0]&B[0];
assign Y[1] = (A[0]|B[0])&(A[1]|B[1])&(~(A[0]&B[0]&A[1]&B[1])); //this is wrong
assign Y[2] = A[1]&B[1]&(~(A[0]&B[0]));
assign Y[3] = A[1]&B[1]&A[0]&B[0];
    
endmodule