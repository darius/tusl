\ The sokoban 'level' (the board).

:example-level  (
"# # # # # # # 
# . i   #   # 
# o @   o   # 
#       o   # 
#   . .     # 
#     @     # 
# # # # # # # 
" constant)

:level-var      (example-level variable)
:level          level-var @ ;

:width          level 0 10 string-c-index 1+ ;


\ The solution record

:moves-size     (4096 constant)
:moves          (here constant  moves-size allot)
:moves-ptr      (0 variable)
:moves-at       moves-ptr @ {p}
                0 p moves-size within (if)  p moves + ;  (then)
                "Moves-record out of bounds" throw ;
:moves-push     moves-at c!  1 moves-ptr +! ;
:moves-pop      -1 moves-ptr +!  moves-at c@ ;
:moves-get      0 moves-at c!  moves ;


\ Squares may be walls ('#') or containers (' ' or '.') with an optional 
\ containee ('o' or 'i').

:kinds          ("# oi.@I" constant)
:containers     ("#   ..." constant)
:containees     ("  oi oi" constant)

:char->kind {c} kinds 0 c string-c-index ;
:parse          char->kind {i}  i 0< (if)  "Unknown kind" throw ;  (then)  i ;
:unparse        kinds + c@ ;

:->container    parse containers + c@ ;
:->containee    parse containees + c@ ;

:clear? {c}     c $  =  c $. = or ;
:pusher? {c}    c $i =  c $I = or ;
:barrel? {c}    c $o =  c $@ = or ;


\ Move a pusher or barrel one step.

:split {c}      c ->containee  c ->container ;
:join {containee container}  container $  = (if)  containee ;  (then)
                             containee parse 3 + unparse ;

:split! {a}     a c@ split  a c! ;
:join! {a}      a c@ join   a c! ;
:plop {a dir}   a split!  a dir + join! ;


\ Move a pair of them together.

:plop-plop {a dir}      a dir + dir plop  a dir plop ;


\ Undoably try to move the pusher, pushing any adjacent unblocked barrel.

:no-move   {a dir c}    ;
:step-move {a dir c}    a dir plop       c moves-push ;
:push-move {a dir c}    a dir plop-plop  c to-uppercase moves-push ;

:move {a dir c}
  a dir c       a dir + c@  a dir 2* + c@  {s0 s1}
                s0 clear?                 (if)  step-move ;  (then)
                s0 barrel?  s1 clear? and (if)  push-move ;  (then)
                                                no-move ;

\ Undo a move.

:step-unmove            plop ;
:push-unmove {a dir}    a dir - dir plop-plop ;

:unmove         uppercase? (if) push-unmove ; (then) step-unmove ;


\ To move or unmove, we need to find the pusher and the displacement.

:at {a}         a c@ pusher? (if)  a ;        (then)
                a c@         (if)  a 1+ at ;  (then)
                "no pusher!" throw ;

:get-displacement 
                to-lowercase {c}
                c $u = (if)  width negate ;  (then)
                c $d = (if)         width ;  (then)
                c $l = (if)            -2 ;  (then)
                c $r = (if)             2 ;  (then)
                "Unknown move" throw ;

:push-it {c}    level at  c get-displacement  c move ;
:undo           moves-ptr @ 0> (when)
                  moves-pop {c}
                  level at  c get-displacement negate  c unmove ;


\ The UI

:solved?        0 $o string-c-index -1 = ;

:at-xy          level at level -  width /mod ;
:string-blast   dup strlen screen-blast ;
:banner         level solved? (if)  "yay" ;  (then)  "   " ;
:update         0 0 banner string-blast
                0 2 level string-blast
                at-xy 2+ screen-refresh ;

:react {key}    key 0x104 = (if)  $l push-it ;  (then)
                key 0x105 = (if)  $r push-it ;  (then)
                key 0x103 = (if)  $u push-it ;  (then)
                key 0x102 = (if)  $d push-it ;  (then)
                key $u    = (if)        undo ;  (then)
                ;
:playing        update
                get-key {key}  key $q = (unless)
                  key react
                  playing ;

:?complain {plaint}
                plaint (when) plaint type cr ;
:play           screen-setup  'playing catch  screen-teardown  ?complain ;


\ Load a level from a file, play it, and append any solution to the same file.

:snarfing       absorb {c}  c 0< (unless)  c c,  snarfing ;
:snarf {filename}
                here level-var !
                filename "r" 'snarfing with-io-on-file  
                0 c, ;

:type-line      type cr ;
:file-append    "a" 'type-line with-io-on-file ;

:play-file {filename}
                filename snarf  play
                level solved? (if)  moves-get filename file-append  (then) ;
