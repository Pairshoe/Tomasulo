addi r1,r1,10 ; r1=10
addi r2,r2,3 ; r2=3
sw r1,r2,1 ; mem[4]=10
label1 add r4,r1,r2 ; r4=13
and r5,r2,r4 ; r5=1
sub r2,r2,r5 ; r2=2
noop
noop
addi r10,r10,5
addi r12,r12,7
loop addi r2,r2,387
andi r2,r2,15
addi r12,r12,1
sw r2,r12,0
addi r10,r10,-1
beqz r10,end
j loop
end noop
noop
noop
lw r13,r12,0
halt