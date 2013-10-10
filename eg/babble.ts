\ Leo Brodie's fog generator, translated from _Starting Forth_.


\ Output with word-wrap

:typing {a u}   u (when)  a c@ emit  a 1+ u 1- typing ;

:column (0 variable)
:passed?        61 column @ < ;  \ Is the column past the right margin?
:newline        cr  0 column ! ;
:space          1 column +!  passed? (if) newline ; (then)  32 emit ;
:?newline       passed? (if) newline (then) ;
:.word {a u}    u column +!  ?newline  a u typing ;

:delim? {c}     c 0=  c bl = or ;
:wording {a u}  a u + c@ delim? (if) a u ; (then)  a u 1+ wording ;
:word {a}       a 0 wording ;

:?space {a}     a c@ 32 = (if) space  a 1+ ; (then) a ;
:spew           word {a u}  u (when)
                  a u .word
                  a u + ?space spew ;


\ The random text snippets that we'll stitch together

:pick {u}       random u umod ;  \ Pick a random number less than u.

\ A table, given an index u, returns the u'th following entry.
:table          given  ;will {u a}  u cells a + @ ;

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


\ Stitching the snippets into a 4-paragraph paper

:s              for ;

:phrase         space filler space noun-phrase ;
:paragraph      intro  4 'phrase s  $. emit newline newline ;
:paper          newline  4 'paragraph s ;

(paper)
