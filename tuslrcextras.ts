\ These were in tuslrc.ts once, but weren't often used.

:over {x y}		x y x ;
:rot {x y z}		y z x ;

:2@ {addr}		addr cell+ @  addr @ ;
:2! 			{addr}  addr !  addr cell+ ! ;

:2,			here 2!  2 cells allot ;

:and-also		'0= , if 'false , '; , then ;
:or-else		if 'true , '; , then ;

:2variable		here constant  2, ;

:2literal {x y}		x literal  y literal ;
:2constant		2literal  '; , ;
