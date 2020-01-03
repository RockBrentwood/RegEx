#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

// Derived from the syntax:
//	Ex -> c | x | '(' Ex ')' | '[' Ex ']' | Ex p | Ex [i] Ex,
//	Ex -> x '=' Ex ',' Ex,		// substitution operator
//	p: '+' '*' '?',			// postfix operators
//	i: (empty) '|' '^' '-' '&',	// infix operators (the empty operator is concatenation)
//	x: Name/Identifier,		// a C-identifier or "..." quoted-name.
//	c: Literal Constant,		// '0' or '1'
//	Ex.				// Top-level expression
// The postfix operators +, *, ? bind more tightly than concatenation
// Concatenation binds more tightly than '&', '^' which both bind more tightly than '|' and '-'
// The infix operators bind more tightly than the assignment operator.
// Concatenation associates to the right: A B C = A (B C)
// The other infix operators associate to the left: A | B | C = A | (B | C), A - B - C = A - (B - C), etc.
// The substitution operator (x = A, B) only has local scope, therefore,
//	(x = y y, x a) | b x means y y a | b x, not y y a | b y y.

void *Allocate(size_t N) {
   void *X = malloc(N);
   if (X == 0) printf("Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

void *Reallocate(void *X, size_t N) {
   X = X == 0? malloc(N): realloc(X, N);
   if (X == 0) printf("Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

char *CopyS(char *S) {
   char *NewS = Allocate(strlen(S) + 1);
   strcpy(NewS, S); return NewS;
}

static int Line = 1;

static int Get(void) {
   int Ch = getchar();
   if (Ch == '\n') Line++;
   return Ch;
}

static void UnGet(int Ch) {
   if (Ch == '\n') --Line;
   ungetc(Ch, stdin);
}

static int Errors = 0;
static void Error(char *Format, ...) {
   const int MaxErrors = 25;
   fprintf(stderr, "[%d] ", Line);
   va_list AP; va_start(AP, Format); vfprintf(stderr, Format, AP); va_end(AP);
   fputc('\n', stderr);
   if (++Errors >= MaxErrors) fprintf(stderr, "Reached the %d error limit.\n", MaxErrors), exit(EXIT_FAILURE);
}

typedef unsigned char byte;
typedef struct Exp *Exp;
typedef struct Symbol *Symbol;
struct Symbol { char *Name; byte Hash; Exp Value; Symbol Next; };

static byte Hash(char *S) {
   byte H = 0;
   for (char *T = S; *T != '\0'; T++) H = (H << 1) ^ *T;
   return H&0xff;
}

static Symbol LookUp(char *Name) {
   static Symbol Table[0x100];
   byte H = Hash(Name);
   for (Symbol Sym = Table[H]; Sym != 0; Sym = Sym->Next) if (strcmp(Sym->Name, Name) == 0) return Sym;
   Symbol Sym = Allocate(sizeof *Sym);
   Sym->Name = CopyS(Name), Sym->Value = 0, Sym->Hash = H, Sym->Next = Table[H], Table[H] = Sym;
   return Sym;
}

static Symbol Sym;
char Scan(void) {
   static char Buf[0x100];
   const char *BufEnd = Buf + sizeof Buf/sizeof Buf[0] - 1;
   char Ch;
   do Ch = Get(); while (isspace(Ch));
   switch (Ch) {
      case EOF: return 0;
      case '-': case '&': case '^':
      case '|': case '(': case ')': case '[': case ']': case '=':
      case '0': case '1': case '*': case '+': case '?': case ',': return Ch;
      case '"': {
         char *BP = Buf;
         for (; (Ch = Get()) != '"' && Ch != EOF; ) if (BP < BufEnd) *BP++ = Ch;
         if (Ch == EOF) printf("Missing closing \".\n"), exit(EXIT_FAILURE);
         *BP = '\0', Sym = LookUp(Buf);
      }
      return 'x';
      default:
         if (!isalpha(Ch)) { Error("Extra character: '%c'.", Ch); return 0; }
      case '_': case '$': {
         char *BP = Buf;
         for (; isalnum(Ch) || Ch == '_' || Ch == '$'; Ch = Get()) if (BP < BufEnd) *BP++ = Ch;
         if (Ch != EOF) UnGet(Ch);
         *BP = '\0', Sym = LookUp(Buf);
      }
      return 'x';
   }
}

typedef struct Term { Symbol X; Exp Q; } *Term;
struct Exp {
   char Tag; byte Hash; int State;
   unsigned long X; byte Stack:1, Normal:1, Unit:1;
   int Terms; Term Sum;
   Exp Next; void *Body;
};
#define Branch(E, N) (((Exp *)((E)->Body))[N])
#define Leaf(E) ((Symbol)((E)->Body))

Exp GetExp(char Tag, ...) {
   static Exp Table[0x100], Zero = 0, One = 0;
   static unsigned long ID = 0;
   Symbol Sym; Exp A, B, E; unsigned H;
   va_list AP; va_start(AP, Tag);
   switch (Tag) {
      case '0': H = 0, E = Zero; break;
      case '1': H = 1, E = One; break;
      case 'x': Sym = va_arg(AP, Symbol); H = Sym->Hash, E = Sym->Value; break;
      case '*': case '+': case '?':
         A = va_arg(AP, Exp), H = A->Hash;
         H = Tag == '?'? 0x02 + (int)7*H/0x80: Tag == '+'? 0x10 + H/8: 0x30 + (int)3*H/0x10;
         for (E = Table[H]; E != 0; E = E->Next) if (A == Branch(E, 0)) break;
      break;
      case '^': case '&': case '.': case '-': case '|':
         A = va_arg(AP, Exp), B = va_arg(AP, Exp), H = A->Hash + B->Hash;
         H = Tag == '&'? 0x60 + H/0x20: Tag == '-'? 0x70 + H/0x20: Tag == '|'? 0x80 + H/8: 0xc0 + H/8;
         for (E = Table[H]; E != 0; E = E->Next) if (A == Branch(E, 0) && B == Branch(E, 1)) break;
      break;
   }
   va_end(AP);
   if (E == 0) {
      E = Allocate(sizeof *E); E->X = ID++;
      E->State = 0, E->Stack = 0, E->Normal = 0;
      switch (E->Tag = Tag) {
         case '0': E->Unit = 0; return Zero = E;
         case '1': E->Unit = 1; return One = E;
         case 'x': E->Unit = 0, E->Body = (void *)Sym, Sym->Value = E; return E;
         case '*': case '?': E->Unit = 1; goto MakeUnary;
         case '+': E->Unit = A->Unit;
         MakeUnary:
            E->Body = Allocate(sizeof(Exp)), Branch(E, 0) = A;
         break;
         case '^': case '&': case '.': E->Unit = A->Unit && B->Unit; goto MakeBinary;
         case '-': E->Unit = A->Unit && !B->Unit; goto MakeBinary;
         case '|': E->Unit = A->Unit || B->Unit;
         MakeBinary:
            E->Body = Allocate(2*sizeof(Exp)), Branch(E, 0) = A, Branch(E, 1) = B;
         break;
      }
      E->Hash = H, E->Next = Table[H], Table[H] = E;
   }
   return E;
}

static Exp *AStack = 0, *BStack = 0;
static int AMax = 0, BMax = 0, As, Bs;
void InsertA(Exp E) {
   if (As >= AMax) AMax += 4, AStack = Reallocate(AStack, AMax*sizeof *AStack);
   AStack[As++] = E;
}
void InsertB(Exp E) {
   if (Bs >= BMax) BMax += 4, BStack = Reallocate(BStack, BMax*sizeof *BStack);
   BStack[Bs++] = E;
}
Exp CatExp(Exp A, Exp B) {
   for (As = 0; A->Tag == '.'; A = Branch(A, 1)) InsertA(Branch(A, 0));
   InsertA(A);
   while (As-- > 0) {
      Exp E = AStack[As];
      if (E->Tag == '0' || B->Tag == '1') B = E;
      else if (E->Tag == '1' || B->Tag == '0') ;
      else B = GetExp('.', E, B);
   }
   return B;
}
Exp BinExp(char Tag, Exp A, Exp B) {
   for (As = 0; A->Tag == Tag; A = Branch(A, 1)) InsertA(Branch(A, 0));
   for (Bs = 0; B->Tag == Tag; B = Branch(B, 1)) InsertB(Branch(B, 0));
   Exp C;
   if (A->X > B->X) C = A, InsertB(B); else C = B, InsertA(A);
   As--, Bs--;
   while (As >= 0 || Bs >= 0) {
      int Diff = 0;
      if (As >= 0) A = AStack[As]; else Diff = -1;
      if (Bs >= 0) B = BStack[Bs]; else Diff = +1;
      if (Diff == 0) Diff = A->X - B->X;
      Exp D;
      if (Diff <= 0) Bs--, D = B; else As--, D = A;
      if (Diff == 0 && Tag != '^') As--;
      switch (Tag) {
         case '|': // 0 | x = x | 0 = x | x = x
            if (C->Tag == '0') { C = D; continue; }
            if (D->Tag == '0' || D == C) continue;
         break;
         case '&': // 0 & x = x & 0 = 0, x & x = x
            if (D->Tag == '0') { C = D; continue; }
            if (C->Tag == '0' || D == C) continue;
         break;
         case '^': // 1^x = x = x^1, x^0 = 0 = 0^x
            if (D->Tag == '0' || C->Tag == '1') { C = D; continue; }
            if (D->Tag == '1' || C->Tag == '0') continue;
         break;
      }
      C = GetExp(Tag, D, C);
   }
   return C;
}

#define STACK_MAX 0x80
char Stack[STACK_MAX], *SP = Stack;
Exp RStack[STACK_MAX], *RSP = RStack;
Symbol LStack[STACK_MAX], *LSP = LStack;
static void Enter(char Tag, ...) {
   static char *SEnd = Stack + sizeof Stack/sizeof *Stack;
   if (SP >= SEnd) Error("Expression too complex ... aborting."), exit(EXIT_FAILURE);
   va_list AP; va_start(AP, Tag);
   switch (*SP++ = Tag) {
      case '^': case '&': case '-': case '|': case '.': *RSP++ = va_arg(AP, Exp); break;
      case '=': *LSP++ = va_arg(AP, Symbol); break;
      case ',': *LSP++ = va_arg(AP, Symbol), *RSP++ = va_arg(AP, Exp); break;
   }
   va_end(AP);
}

static char Leave(char L) {
   unsigned P;
   switch (L) {
      default: P = 0; break;
      case '&': case '^': P = 1; break;
      case '|': case '-': P = 2; break;
      case '=': case ',': P = 3; break;
      case ')': case ']': P = 4; break;
      case 0: P = 5; break;
   }
   char Tag = *--SP;
   switch (Tag) {
      case '^': case '&': case '.': if (P > 0) { RSP--; return Tag; } break;
      case '|': case '-': if (P > 1) { RSP--; return Tag; } break;
      case '=': if (P > 2) { LSP--; return Tag; } break;
      case ',': if (P > 2) { RSP--, LSP--; return Tag; } break;
      case '(': case '[': if (P > 3) return Tag; break;
      default: if (P > 4) return Tag; break;
   }
   SP++; return '\0';
}

Exp Parse(void) {
   char Act; Exp E;
   Enter('$');
   char L = Scan();
Start:
   switch (L) {
      case '(': case '[': Enter(L), L = Scan(); goto Start;
      case '0': case '1': E = GetExp(L), L = Scan(); goto Reduce;
      case 'x': {
         Symbol X = Sym;
         if ((L = Scan()) != '=') { E = GetExp('x', X); goto Reduce; }
         Enter('=', X), L = Scan();
      }
      goto Start;
      default: Error("Corrupt expression."); return 0;
   }
Reduce:
   switch (Act = Leave(L)) {
      case '$': goto End;
      case '=': {
         Symbol X = *LSP;
         if (L == ',') L = Scan(); else Error("Missing ','.");
         Enter(',', X, X->Value), X->Value = E;
      }
      goto Start;
      case ',': (*LSP)->Value = *RSP; goto Reduce;
      case '[':
         if (L == ']') L = Scan(); else Error("Unmatched '['.");
         E = GetExp('?', E);
      goto Reduce;
      case '(':
         if (L == ')') L = Scan(); else Error("Unmatched '('.");
      goto Reduce;
      case '.': case '|': case '^': case '&': case '-': E = GetExp(Act, *RSP, E); goto Reduce;
   }
Shift:
   switch (L) {
      default: Enter('.', E); goto Start;
      case '|': case '^': case '&': case '-': Enter(L, E), L = Scan(); goto Start;
      case '*': case '+': case '?': E = GetExp(L, E), L = Scan(); goto Reduce;
      case '=': Error("Left-hand side of '=' must be a name."); exit(EXIT_FAILURE);
      case ',': Error("Extra ','"), L = Scan(); goto Reduce;
      case ')': case ']': Error("Unmatched %c.", L), L = Scan(); goto Reduce;
   }
End:
   return E;
}

Exp *XStack = 0; int Xs = 0, XMax = 0;
void PushQ(Exp Q) {
   if (Q->Stack) return;
   if (Xs >= XMax) XMax += 0x10, XStack = Reallocate(XStack, XMax*sizeof *XStack);
   XStack[Xs++] = Q; Q->Stack = 1;
}

void PopQ(void) { XStack[--Xs]->Stack = 0; }

Exp *STab; int Ss;
void AddState(Exp Q) {
   if (Q->State > 0) return;
   if ((Ss&7) == 0) STab = Reallocate(STab, (Ss + 8)*sizeof *STab);
   Q->State = Ss; STab[Ss++] = Q;
}

Term TList; int Ts, TMax;
void AddTerm(Symbol X, Exp Q) {
   if (Ts >= TMax) TMax += 4, TList = Reallocate(TList, TMax*sizeof *TList);
   TList[Ts].X = X, TList[Ts].Q = Q, Ts++;
}

void FormStates(Exp E) {
   Exp A, B; int AX, BX;
   STab = 0, Ss = 0; AddState(GetExp('0')); AddState(E);
   TList = 0, TMax = 0;
   for (int S = 0; S < Ss; S++) {
      PushQ(STab[S]);
      while (Xs > 0) {
         E = XStack[Xs - 1];
         if (E->Normal) { PopQ(); continue; }
         switch (E->Tag) {
            case '0': case '1': // d0 = d1 = 0
               E->Terms = 0, E->Sum = 0, E->Normal = 1; PopQ();
            break;
            case 'x': // dx = x
               E->Terms = 1, E->Sum = Allocate(sizeof *E->Sum);
               E->Sum[0].X = Leaf(E), E->Sum[0].Q = GetExp('1');
               E->Normal = 1; PopQ();
            break;
            case '?': // d(A?) = dA
               A = Branch(E, 0); if (!A->Normal) { PushQ(A); continue; }
               E->Terms = A->Terms, E->Sum = A->Sum;
               E->Normal = 1; PopQ();
            break;
            case '*': case '+': // d(A*) = d(A+) = dA A*
               A = Branch(E, 0);
               if (!A->Normal) { PushQ(A); continue; }
               B = E->Unit? E: GetExp('*', A);
            Catenate:
               E->Terms = A->Terms;
               E->Sum = Allocate(E->Terms*sizeof *E->Sum);
               for (int I = 0; I < E->Terms; I++) E->Sum[I].X = A->Sum[I].X, E->Sum[I].Q = CatExp(A->Sum[I].Q, B);
               E->Normal = 1; PopQ();
            break;
            case '.': case '|': case '&': case '-': case '^':
               A = Branch(E, 0); if (!A->Normal) { PushQ(A); continue; }
               B = Branch(E, 1);
               if (E->Tag == '.' && !A->Unit) goto Catenate;
               if (!B->Normal) { PushQ(B); continue; }
               for (AX = BX = Ts = 0; AX < A->Terms || BX < B->Terms; ) {
                  Exp dA, dB; Symbol xA, xB;
                  int Diff = 0;
                  if (AX >= A->Terms) Diff = 1; else dA = A->Sum[AX].Q, xA = A->Sum[AX].X;
                  if (BX >= B->Terms) Diff = -1; else dB = B->Sum[BX].Q, xB = B->Sum[BX].X;
                  if (Diff == 0) Diff = strcmp(xA->Name, xB->Name);
                  if (Diff <= 0) AX++;
                  if (Diff >= 0) BX++;
                  switch (E->Tag) {
                     case '&':
                        if (Diff == 0) AddTerm(xA, BinExp('&', dA, dB));
                     break;
                     case '.':
                        if (Diff <= 0) dA = CatExp(dA, B);
                     goto Join;
                     case '^':
                        if (Diff <= 0) dA = BinExp('^', dA, B);
                        if (Diff >= 0) dB = BinExp('^', dB, A);
                     case '|': Join:
                        if (Diff < 0) AddTerm(xA, dA);
                        else if (Diff > 0) AddTerm(xB, dB);
                        else AddTerm(xA, BinExp('|', dA, dB));
                     break;
                     case '-':
                        if (Diff == 0) dA = GetExp('-', dA, dB);
                        if (Diff <= 0) AddTerm(xA, dA);
                     break;
                  }
               }
               E->Terms = Ts, E->Sum = Allocate(E->Terms*sizeof *E->Sum);
               for (int I = 0; I < Ts; I++) E->Sum[I] = TList[I];
               E->Normal = 1; PopQ();
            break;
         }
      }
      E = STab[S];
      for (int I = 0; I < E->Terms; I++) AddState(E->Sum[I].Q);
   }
   free(XStack), XMax = 0; free(TList), TMax = 0;
}

static struct Equiv { Exp L, R; } *ETab; int Es, EMax;

void AddEquiv(Exp L, Exp R) {
   if (L == R) return;
   if (L->State > R->State) { Exp Q = L; L = R, R = Q; }
   for (int E = 0; E < Es; E++) if (L == ETab[E].L && R == ETab[E].R) return;
   if (Es >= EMax) EMax += 8, ETab = Reallocate(ETab, EMax*sizeof *ETab);
   ETab[Es].L = L, ETab[Es].R = R, Es++;
}

void MergeStates(void) {
   ETab = 0, EMax = 0;
   for (int S = 1; S < Ss; S++) {
      Exp Q = STab[S];
      if (Q->State < S) continue;
      int S1 = 0;
      for (; S1 < S; S1++) {
         int LX, RX;
         Exp Q1 = STab[S1]; if (Q1->State < S1) continue;
         Es = 0, AddEquiv(Q, Q1);
         for (int E = 0; E < Es; E++) {
            Exp L = ETab[E].L, R = ETab[E].R;
            if (L->Unit != R->Unit) goto NOT_EQUAL;
            for (LX = 0, RX = 0; LX < L->Terms && RX < R->Terms; ) {
               int Diff = strcmp(L->Sum[LX].X->Name, R->Sum[RX].X->Name);
               if (Diff < 0) AddEquiv(STab[0], L->Sum[LX++].Q);
               else if (Diff > 0) AddEquiv(STab[0], R->Sum[RX++].Q);
               else AddEquiv(L->Sum[LX++].Q, R->Sum[RX++].Q);
            }
            for (; LX < L->Terms; LX++) AddEquiv(STab[0], L->Sum[LX].Q);
            for (; RX < R->Terms; RX++) AddEquiv(STab[0], R->Sum[RX].Q);
         }
      EQUAL: break;
      NOT_EQUAL: continue;
      }
      if (S1 < S) for (int E = 0; E < Es; E++) ETab[E].R->State = ETab[E].L->State;
   }
   int Classes = 0;
   for (int S = 0; S < Ss; S++) {
      Exp Q = STab[S];
      Q->State = (Q->State < S)? STab[Q->State]->State: Classes++;
   }
}

void WriteStates(void) {
   int Classes = 1;
   for (int S = 1; S < Ss; S++) {
      Exp Q = STab[S];
      if (Q->State < Classes) continue;
      Classes++;
      printf("Q%d =", Q->State);
      int I = 0;
      for (; I < Q->Terms; I++) if (Q->Sum[I].Q->State != 0) break;
      if (Q->Unit) {
         printf(" 1");
         if (I < Q->Terms) printf(" |");
      }
      for (; I < Q->Terms; ) {
         printf(" %s Q%d", Q->Sum[I].X->Name, Q->Sum[I].Q->State);
         for (I++; I < Q->Terms; I++) if (Q->Sum[I].Q->State != 0) break;
         if (I < Q->Terms) printf(" |");
      }
      putchar('\n');
   }
   if (Classes < 2) printf("Q0 = 0\n");
}

int main(void) {
   Exp E = Parse();
   if (Errors > 0) fprintf(stderr, "%d error(s)\n", Errors);
   if (E == 0) return EXIT_FAILURE;
   FormStates(E); MergeStates(); WriteStates();
   return EXIT_SUCCESS;
}
