var(exists,x1,1).
var(exists,x2,2).
var(exists,x3,3).
var(exists,x4,4).
var(exists,x5,5).
var(all,y1,1).
var(all,y2,2).
var(all,y3,3).
var(all,y4,4).
var(all,y5,5).
k(16).

        {true(exists,X,C)} :- var(exists,X,C).
        :- not saturate.
        true(all,X,C) :- var(all,X,C), saturate.
        saturate :- f_sum(0,"!=",K), k(K).
        f_set(0,C,true(exists,X,C)) :- var(exists,X,C).
        f_set(0,C,true(all,X,C)) :- var(all,X,C).
    
