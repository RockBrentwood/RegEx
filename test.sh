CompareFA() {
   ./$1 <$2.in >$3.ex
   if ! cmp -s $3.ex Ref/$3.ex; then
      echo === $3.ex ===
      diff -d $3.ex Ref/$3.ex
   else
      rm $3.ex
   fi
}
CompareFSC() {
   ./$1 <$2.in >$2.ex >&$2.lg
   if ! cmp -s $2.ex Ref/$2.ex; then
      echo === $2.ex ===
      diff -d $2.ex Ref/$2.ex
   else
      rm $2.ex
   fi
   if ! cmp -s $2.lg Ref/$2.lg; then
      echo === $2.lg ===
      diff -d $2.lg Ref/$2.lg
   else
      rm $2.lg
   fi
}
CompareREX() {
   ./$1 -f $2.in <$2.txt >$2.ex
   if ! cmp -s $2.ex Ref/$2.ex; then
      echo === $2.ex ===
      diff -d $2.ex Ref/$2.ex
   else
      rm $2.ex
   fi
}

CompareFA "dfa" "dfa0" "dfa0D"
CompareFA "dfa" "dfa1" "dfa1D"
CompareFA "dfa" "dfa2" "dfa2D"
CompareFA "dfa" "fa0" "dfa0"
CompareFA "dfa" "fa1" "dfa1"
CompareFA "dfa" "fa2" "dfa2"
CompareFA "dfa" "fa3" "dfa3"
CompareFA "dfa" "fa4" "dfa4"
CompareFA "dfa" "fa5" "dfa5"
CompareFA "dfa" "fa6" "dfa6"
CompareFA "dfa" "fa7" "dfa7"
CompareFA "dfa" "fa8" "dfa8"
CompareFA "dfa" "fa9" "dfa9"
CompareFA "nfa" "fa0" "nfa0"
CompareFA "nfa" "fa1" "nfa1"
CompareFA "nfa" "fa2" "nfa2"
CompareFA "nfa" "fa3" "nfa3"
CompareFA "nfa" "fa4" "nfa4"
CompareFA "nfa" "fa5" "nfa5"
CompareFA "nfa" "fa6" "nfa6"
CompareFA "nfa" "fa7" "nfa7"
CompareFA "nfa" "fa8" "nfa8"
CompareFA "nfa" "fa9" "nfa9"
CompareFSC "fsc" "fsc0"
CompareFSC "fsc" "fsc1"
CompareFSC "fsc" "fsc2"
CompareFSC "fsc" "fsc3"
CompareFSC "fsc" "fsc4"
CompareFSC "fsc" "fsc5"
CompareFSC "fsc" "fsc6"
CompareFSC "fsc" "fsc7"
CompareFSC "fsc" "fsc8"
CompareFSC "fsc" "fsc9"
CompareREX "rex" "rex0"
CompareREX "rex" "rex1"
