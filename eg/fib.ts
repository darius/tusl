\ Recursive Fibonacci benchmark

:fib {n}	n 2 < (if)  1 ;
		    (then)  n 1- fib  n 2- fib + ;

(25 fib . cr
