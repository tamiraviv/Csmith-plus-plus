/*
	this follow code doesn't compile in intel, but does in gcc
	intel: error: "E::f" is ambiguous
*/

struct A {
   int  f;
};

struct B: A {

};

struct C: virtual A, virtual B {

};

struct D: B , C {
   int  f;
};

struct E: D, C, virtual A
{

};

/* ---------------------------------------- */
int main (int argc, char* argv[])
{
    E e;
	e.f;
    return 0;
}


/*

1. struct A { int f }  // {{A::f} {A} }
2. struct B: A{} //  {{A::f} {A} }
3. struct C: virtual A, virtual B{} // = {{A1::f} {A1} } ---merge---->    { invalid, { A1, A2 } }
4. struct D: B,C {int f} // = {{D::f} {D} } } 
5. struct E: D,C,Virtual A // {{D::f} {D} } } ---first merge---->   {{D::f {D} } } ---secound merge---->   {{D::f} {D} } }  

explain eash calculation from the standard( we are calculate for every struct S(f,C)):

1.  If A contains a declaration of the name f, the declaration set contains every declaration of f declared in
	A that satisfies the requirements of the language construct in which the lookup occurs
	If the resulting declaration set is not empty, the subobject set contains A
	itself, and calculation is complete.

2.  if B does not contain a declaration of f or the resulting declaration set is empty - S(f,B) is initially empty.
	and for the merging with A : if S(f,B) is empty, the new S(f,B) is a copy of S(f,A).
	
3.  if C does not contain a declaration of f or the resulting declaration set is empty - S(f,C) is initially empty.
	and for the merging with A : if S(f,C) is empty, the new S(f,C) is a copy of S(f,A).
	and for the merging with B : S(f,C) = S(f,A) = {{A1::f} {A1}  , and S(f,B) = {{A2::f} {A2}
		so: if the declaration sets of S(f,B) and S(f,C) differ, the merge is ambiguous: the new
		S(f,C) is a lookup set with an invalid declaration set and the union of the subobject sets. In subsequent
		merges, an invalid declaration set is considered different from any other.

4.  If D contains a declaration of the name f, the declaration set contains every declaration of f declared in
	D that satisfies the requirements of the language construct in which the lookup occurs
	If the resulting declaration set is not empty, the subobject set contains D
	itself, and calculation is complete.		

5.  if E does not contain a declaration of f or the resulting declaration set is empty - S(f,E) is initially empty.
	and for the merging with D :if S(f,E) is empty, the new S(f,E) is a copy of S(f,D).
	and for the merging with C :
		If each of the subobject members of S(f,C) is a base class subobject of at least one of the subobject
		members of S(f,E),  S(f,E) is unchanged and the merge is complete (A is a base class subobject of D)
	and for the merging with A :
		If each of the subobject members of S(f,A) is a base class subobject of at least one of the subobject
		members of S(f,E),  S(f,E) is unchanged and the merge is complete (A is a base class subobject of D)




*/