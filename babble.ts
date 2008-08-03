\ Leo Brodie's fog generator, translated from _Starting Forth_.

:pick z-        random z umod ;

:table          given ;will yz- y 4* z + @ ;

:repeat xyz-    y z < (when)  y x execute       \ Call x on each of y..!z.
                x y 1+ z repeat ;
:plural         given ,                         \ Will call the arg on 0..!z.
                ;will yz-  z @ 0 y repeat ;

:typing yz-     z (when)  y c@ emit  y 1+ z 1- typing ;

:column (0 variable)
:over?          61 column @ < ;
:newline        cr  0 column ! ;
:space          1 column +!  over? (if) newline ; (then)  32 emit ;
:?newline       over? (if) newline (then) ;
:.word yz-      z column +!  ?newline  y z typing ;

:wording yz-    z y + c@   0= (if) z y ; (then)
                z y + c@ 32 = (if) z y ; (then)
                y 1+ z wording ;
:word z-        0 z wording ;

:?space z-      z c@ 32 = (if) space  z 1+ ; (then) z ;
:spew           word yz-  z (when)      
                y z .word
                y z + ?space spew ;
\ That line-breaking is still fucked up somehow...

:intros (table 
   "In this paper we will demonstrate that" ,
   "On the one hand, studies have shown that" ,
   "On the other hand, however, practical experience indicates that" ,
   "In summary, then, we propose that" ,
:intro          intros spew ;

:fillers (table
   "by using" ,                 
   "by applying available resources towards" ,
   "with structured deployment of" ,

   "coordinated with" ,
   "to offset" ,
   "balanced by" , 
   
   "it is possible for even the most" ,
   "it becomes not unfeasible for all but the least" ,
   "it is necessary for all" ,
   
   "to function as" ,
   "to generate a high level of" ,
   "to avoid" ,
:filler         3 * 3 pick + fillers spew ;

:1st-adjective (table
  "integrated",         "total",        "systematized", "parallel",
  "functional",         "responsive",   "optimal",      "synchronized",
  "compatible",         "qualified",    "partial",      "standalone",
  "random",             "representative","optional",    "transient",
:2nd-adjective (table
  "management",         "organization", "monitored",    "reciprocal",  
  "digital",            "logistical",   "transitional", "incremental",
  "third generation",   "policy",       "decision",     "undocumented",
  "context-sensitive",  "fail-safe",    "omni-range",   "unilateral",
:noun (table
  "criteria",           "flexibility",  "capability",   "mobility",
  "programming",        "concepts",     "time phasing", "projections",
  "hardware",           "throughput",   "engineering",  "outflow",
  "superstructures",    "interaction",  "congruence",   "utilities",
:noun-phrase    16 pick 1st-adjective spew  space
                16 pick 2nd-adjective spew  space
                16 pick noun          spew ;

:phrase         space filler space noun-phrase ;
:phrases        ('phrase plural)
:paragraph      intro 4 phrases  $. emit newline newline ;

:paragraphs     ('paragraph plural)
:paper          newline 4 paragraphs ;          \ Babble a paper!
(paper)
