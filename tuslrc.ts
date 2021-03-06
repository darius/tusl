\ Basics

\ We make constant words out of these common numbers for efficiency --
\ a reference to a constant takes less space than a literal.
(-1) :-1                (constant)
( 0)  :0                (constant)
( 1)  :1                (constant)

:false                  (0 constant)
:true                   (-1 constant)

:drop {x}               ;
:dup {x}                x x ;
:swap {x y}             y x ;

:cell+                  1 cells + ;   \ XXX worth making primitive?
:cell-                  1 cells - ;

:negate {n}             0 n - ;

:/mod {n d}             n d mod  n d / ;

:0> {n}                 0 n < ;
:<= {m n}               n m < 0= ;
:within {lo n hi}       n lo -  hi lo - u< ;  \ XXX {n lo hi} is better

:c,                     here c!  1 allot ;

:if                     '<<branch>> ,  here  0 , ;
:then {addr}            here addr ! ;

:unless                 if '; , then ;
:when                   '0= , unless ;

:for-range {i n w}      i n < (when)  i w execute  i 1+ n w for-range ;
:for {n w}              0 n w for-range ;

:variable               here constant  , ;

:given                  0 , ;   \ Reserve a cell for ;will data

:abs {n}                n 0< (if) 0 n - ; (then) n ;
:min {m n}              m n < (if) m ; (then) n ;
:max {m n}              n m < (if) m ; (then) n ;

:bl                     ($  constant)
:space                  bl emit ;
:cr                     10 emit ;
:type {str}             str c@ (when)  str c@ emit  str 1+ type ;
:?                      @ . ;

:uppercase? {c}         $A c $Z 1+ within ;
:lowercase? {c}         $a c $z 1+ within ;
:to-lowercase           dup uppercase? (if)  32 +  (then) ;
:to-uppercase           dup lowercase? (if)  32 -  (then) ;
 

:strlen+ {n str}        str c@ (if) n 1+ str 1+ strlen+ ; (then)  n ;
:strlen {str}           0 str strlen+ ;

:strcpy {dest src}      src c@ 
                          (if) src c@ dest c!  dest 1+ src 1+ strcpy ;
                        (then)      0 dest c! ;

:strcat {dest src}      dest dest strlen +  src strcpy ;

:memcpy {dest src n}    n (when)  src c@ dest c!
                                  dest 1+ src 1+ n 1- memcpy ;
:memcpy-up {dest src n} n (when)  src n 1- + c@ dest n 1- + c!  
                                  dest src n 1- memcpy-up ;
:memmove {dest src n}   dest src n  src dest src n + within (if) memcpy-up ;
                                                          (then) memcpy ;

:erase                  0 \ fall through
:fill {str n c}         n (when)  c str c!  str 1+ n 1- c fill ;


:string-c-index {a i c} a i + c@ c = (if)  i ;  (then)
                        a i + c@ 0=  (if) -1 ;  (then)
                        a i 1+ c string-c-index ;


:random-seed            (1234567 variable)
\ XXX wrong: you need u*/mod instead to keep the 64-bit intermediate.
\ https://en.wikipedia.org/wiki/Lehmer_random_number_generator
:random                 random-seed @  16807 u*  2147483647 umod  random-seed !
                        random-seed @ ;


\ Hex/ASCII dump

:hex-digit              0xf and  "0123456789abcdef" + c@ ;
:.hex-digit             hex-digit emit ;
:.hex {u digits}        digits (when)  u 4 u>> digits 1- .hex  u .hex-digit ;
:.byte                  2 .hex ;
:.address               8 .hex ;  \XXX make leading 0s into spaces

:dump-byte {a A Z in out} A a Z within (if) a c@ in execute ;
                                     (then) out execute ;
:dump-line {a i A Z in out}
                        i 16 < (when)
                          i 8 = (if) space (then)
                          a i + A Z in out dump-byte
                          a i 1+ A Z in out dump-line ;

:.byte-in               space .byte ;
:.byte-out              space space space ;
:dump-bytes {a A Z}     a 0 A Z '.byte-in '.byte-out dump-line ;

:printable {c}          32 c 127 within (if) c ; (then) $. ;
:.printable             printable emit ;
:dump-chars {a A Z}     a 0 A Z '.printable 'space dump-line ;

:dump {a u}             a -16 and  a  a u +  \ fall through
:dumping {a A Z}        a Z u< (when)
                           a .address  $: emit
                           a A Z dump-bytes  space space
                           a A Z dump-chars  cr
                           a 16 + A Z dumping ;

\ Stuff for Alph

:reading                absorb {addr char}
                        char 0< (if)  0 addr c! ;  (then)
                                      char addr c!  addr 1+ reading ;

:read-line              absorb {addr char}
                        char 0<   (if)  0 addr c! ;  (then)
                        char 10 = (if)  0 addr c! ;  (then)
                                        char addr c!  addr 1+ read-line ;

:here-constant          here constant  here strlen 1+ allot ;

:read-buf               here reading    here-constant ;
:read-line-buf          here read-line  here-constant ;

:read-here              here reading  here ;

:read-integer           \ fall through...
:read-number            here read-line  
                        here parse-number (unless) drop 0 ;

\ XXX interpret the whole line
:interpret              find drop execute ;
\ XXX use pad instead of here
:run-line               here read-line  here interpret ;


:?throw {error? complaint}
                        error? (when) complaint throw ;


\ Number pairs (rarely used)

:2@ {addr}              addr cell+ @  addr @ ;
:2!                     {addr}  addr !  addr cell+ ! ;

:2,                     here 2!  2 cells allot ;

:2variable              here constant  2, ;

:2literal {x y}         x literal  y literal ;
:2constant              2literal  '; , ;


\ Extras I practically never use, it turns out

:over {x y}             x y x ;
:rot {x y z}            y z x ;

:&&                     '0= , if 'false , '; , then ;
:||                     if 'true , '; , then ;
