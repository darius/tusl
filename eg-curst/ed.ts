:-trailing      bl
:<skip {a u c}  u 0=             (if)  a u ;  (then)
                a u 1- + c@  c - (if)  a u ;  (then)
                                       a u 1- c <skip ;

:allot-filled   here {u c a}  u allot  a u c fill ;

:total-pages    (8 constant)

:ctrl           64 - ;

:filename       (0 variable)


(screen-setup)
(screen-size :height (1- constant) :width (constant)
(screen-teardown)

:messages       (here constant  width bl allot-filled)

:notify {str}   messages width bl fill
                messages  str  str strlen  memcpy ;

:.digit {b d}   b  2 d - 4* u>>  hex-digit  messages d + c! ;
:.notify {b}    "" notify  b 0 .digit  b 1 .digit  b 2 .digit ;

:page-size      (width height * constant)
:pages          (here constant  page-size total-pages * bl allot-filled)
:page           (0 variable)
:page-clip      0 max  total-pages 1- min ;
:page-move      page @ + page-clip  page ! ;
:buffer         page @ page-size * pages + ;

:point          (0 variable)
:clip           0 max  page-size 1- min ;
:+point         point @ +  clip ;
:move           +point  point ! ;
:at             point @  buffer + ;
:+at            +point  buffer + ;

:coords         point @ width /mod ;
:column         coords {c r} c ;
:row            coords {c r} r ;
:rcolumn        width column - ;
:rrow           height row - ;

:bowdlerize {k}       32 k 127 within (if) k ; (then) bl ;

:replace        at c!  1 move ;
:blanks {n}     n 0> (when)  bl replace  n 1- blanks ;

:scrunch        at 1+  at  rcolumn 1-  memmove ;
:insert {k}     scrunch  k replace ;

:tab            bl insert  column 7 and (when) tab ;

:next-row       at rcolumn + ;
:scroll-down    next-row width +  next-row  rrow 2- width * memmove
                next-row width bl fill ;
:newline        row height 1- < (when)
                  scroll-down  next-row at rcolumn memmove  rcolumn blanks ;

:delete         at  rcolumn 1-  {a r}
                a a 1+ r memmove   \XXX what if at start of line?
                bl  a r + c! ;
:backspace      -1 move  delete ;

:kill-line      at rcolumn bl fill ;

:start-of-line        column negate move ;
:end-of-line    start-of-line  
                at width -trailing {a u}  u move ;

:forward-char    1 move ;
:backward-char  -1 move ;
:forward-line   width move ;
:backward-line  width negate move ;

:transpose-chars at c@  -1 +at c@  at c!  -1 +at c!
                 1 move ;

:home           0 point ! ;
:end            buffer page-size -trailing {a u}  u point ! ;

:forward-page    1 page-move  home ;
:backward-page  -1 page-move  home ;


:copying        absorb {c}  c 0< (unless)  c emit  copying ;
:copy-to-file   "wb" 'copying with-io-on-file ;
:copy-file {infile outfile}
                outfile  infile "rb" 'copy-to-file with-io-on-file ;

:pad            here 512 + ;    \ XXX ensure this is available

:backup-filename pad swap strcpy  pad "~" strcat  pad ;

:backup         dup backup-filename copy-file ;

:pristine?      (true variable)
:?backup        pristine? @ (when) filename @ backup  false pristine? ! ;

:au-type {a u}  u (when)  a c@ emit  a 1+ u 1- au-type ;

:visible-height end row 1+ ;

:save-line      width * point !  at width -trailing au-type  10 emit ;
:save-page      page !  visible-height 'save-line for 
                $L ctrl emit ;
:saving         total-pages 'save-page for ;
:save           ?backup
                filename @ "w" 'saving with-io-on-file 
                0 page !  0 point ! ; \XXX keep page&point

:snarf1 {c}     c       9 = (if)  tab ;             (then)
                c      10 = (if)  newline ;         (then)
                c $L ctrl = (if)  forward-page ;    (then)
                                  c bowdlerize insert ;
:snarfing       absorb {c}  c 0< (unless)  c snarf1  snarfing ;
:snarf          "r" 'snarfing with-io-on-file
                0 page !  home ;

:react {k}    k $A ctrl = (if)  start-of-line ;   (then)
                k $B ctrl = (if)  backward-char ;   (then)
                k $D ctrl = (if)  delete ;          (then)
                k $E ctrl = (if)  end-of-line ;     (then)
                k $F ctrl = (if)  forward-char ;    (then)
                k $I ctrl = (if)  tab ;             (then)
                k $K ctrl = (if)  kill-line ;       (then)
                k $M ctrl = (if)  newline ;         (then)
                k $N ctrl = (if)  forward-line ;    (then)
                k $P ctrl = (if)  backward-line ;   (then)
                k $S ctrl = (if)  save ;            (then)
                k $T ctrl = (if)  transpose-chars ; (then)
                k   0x07f = (if)  backspace ;       (then)
                k   0x441 = (if)  backward-line ;   (then)
                k   0x442 = (if)  forward-line ;    (then)
                k   0x443 = (if)  forward-char ;    (then)
                k   0x444 = (if)  backward-char ;   (then)
                k   0x201 = (if)  home ;            (then)
                k   0x203 = (if)  delete ;          (then)
                k   0x204 = (if)  end ;             (then)
                k   0x205 = (if)  backward-page ;   (then)
                k   0x206 = (if)  forward-page ;    (then)
                                  k bowdlerize insert ;

\TO DO:
\ saving/restoring
\ tab C-q C-r C-s C-w C-y C-z
\ M-< M-> M-c M-d M-l M-t M-u M-w M-z 
\ M-left M-right M-up M-down M-a M-e 

:blast-line {i}       0  i  i width * buffer +  width  screen-blast ;
:blast-page     height 'blast-line for 
                0 height messages width screen-blast ;

:editing        blast-page  coords screen-refresh
                get-key {key}  key $Q ctrl = (unless)
                  key react
\                  key .notify
                  editing ;

:?complain {plaint}
                plaint (when) plaint type ;

:edit {file}    file filename !  true pristine? !  file snarf
                screen-setup 'editing catch screen-teardown 
                ?complain ;
