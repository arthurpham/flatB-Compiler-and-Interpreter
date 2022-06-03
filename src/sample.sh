./bcc ../test-units/17_bubble_sort.b codegeneration 2> 17_bubble_sort.ir
llvm-as  17_bubble_sort.ir -o 17_bubble_sort
time lli 17_bubble_sort.ir
chmod +x 17_bubble_sort
time ./17_bubble_sort
time ./bcc ../test-units/17_bubble_sort.b interpret