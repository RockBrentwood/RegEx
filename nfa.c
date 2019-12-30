#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

// Derived from the syntax:
//	Ex -> c | x | '(' Ex ')' | '[' Ex ']' | Ex p | Ex [i] Ex,
//	Ex -> x '=' Ex ',' Ex,		// substitution operator
//	p: '+' '*' '?',			// postfix operators
//	i: (empty) '|',			// infix operators (the empty operator is concatenation)
//	x: Name/Identifier,		// a C-identifier or "..." quoted-name.
//	c: Literal Constant,		// '0' or '1'
//	Ex.				// Top-level expression
// The postfix operators +, *, ? bind more tightly than concatenation
// Concatenation binds more tightly than '|'
// The infix operators are right-associative: A B C = A (B C), A | B | C = A | (B | C)
// The substitution operator (x = A, B) only has local scope, therefore,
//	(x = y y, x a) | b x means y y a | b x, not y y a | b y y.

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

const int MaxErrors = 25;
static int Errors = 0;
static void Error(char *Format, ...) {
   fprintf(stderr, "[%d] ", Line);
   va_list AP; va_start(AP, Format); vfprintf(stderr, Format, AP); va_end(AP);
   fputc('\n', stderr);
   if (++Errors == MaxErrors)
      fprintf(stderr, "Reached the %d error limit.\n", MaxErrors), exit(EXIT_FAILURE);
}

char *LastW;
char Scan(void) {
   static char Buf[0x100];
   const char *End = Buf + sizeof Buf/sizeof Buf[0] - 1;
   char Ch = Get();
   while (isspace(Ch)) Ch = Get();
   switch (Ch) {
      case EOF: return '\0';
      case '|': case '(': case ')': case '[': case ']': case '=':
      case '0': case '1': case '*': case '+': case '?': case ',': return Ch;
      case '"': {
         char *BP = Buf;
         for (; (Ch = Get()) != '"' && Ch != EOF; ) if (BP < End) *BP++ = Ch;
         if (Ch == EOF) printf("Missing closing \".\n"), exit(EXIT_FAILURE);
         *BP = '\0', LastW = Buf;
      }
      return 'x';
      default:
         if (!isalpha(Ch)) { Error("Extra character: '%c'.", Ch); return 0; }
      case '_': case '$': {
         char *BP = Buf;
         for (; isalnum(Ch) || Ch == '_' || Ch == '$'; Ch = Get()) if (BP < End) *BP++ = Ch;
         if (Ch != EOF) UnGet(Ch);
         *BP = '\0', LastW = Buf;
      }
      return 'x';
   }
}

typedef unsigned char byte;
typedef struct Var *Var;
typedef struct Exp *Exp;

void *Allocate(unsigned Bytes) {
   void *X = malloc(Bytes);
   if (X == 0) printf("Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

void *Reallocate(void *X, unsigned Bytes) {
   X = X == 0? malloc(Bytes): realloc(X, Bytes);
   if (X == 0) printf("Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

char *CopyS(char *S) {
   char *NewS = Allocate(strlen(S) + 1);
   strcpy(NewS, S); return NewS;
}

byte Hash(char *S) {
   int H = 0;
   for (; *S != '\0'; S++) H = (H << 1)^*S;
   return H&0xff;
}

struct Var {
   char Tag; int Hash; Exp Class; Var Tail;
   union { char *Leaf; Exp *Arg; } Body;
};

static Var ETab[0x200];
const unsigned E_MAX = sizeof ETab/sizeof ETab[0];

struct Exp {
   Var Value; int Hash, Q; unsigned Mark:1, Stack:1;
};

int Join(int A, int B) {
   long L, S = A + B;
   if (S < E_MAX) L = S*(S + 1)/2 + A;
   else {
      S = 2*(E_MAX - 1) - S, A = E_MAX - 1 - A;
      L = S*(S + 1)/2 + A, L = E_MAX*E_MAX - 1 - L;
   }
   return (int)(L/E_MAX);
}

Var MakeVar(char *Name) {
   int H = 0x100 + Hash(Name); Var E = ETab[H];
   for (; E != 0; E = E->Tail) if (strcmp(Name, E->Body.Leaf) == 0) break;
   if (E == 0) {
      Exp X = Allocate(sizeof *X);
      X->Hash = H, X->Q = -1, X->Mark = 0, X->Stack = 0;
      X->Value = E = (Var)Allocate(sizeof *E);
      E->Tag = 'x', E->Body.Leaf = CopyS(Name), E->Class = X;
      E->Hash = H, E->Tail = ETab[H], ETab[H] = E;
   }
   return E;
}

Exp MakeExp(Exp X, char Tag, ...) {
   char *Name; int H; byte Args; Var E; Exp X0, X1;
   va_list AP; va_start(AP, Tag);
   switch (Tag) {
      case 'x':
         Name = va_arg(AP, char *);
         H = 0x100 + Hash(Name); Args = 0;
         for (E = ETab[H]; E != 0; E = E->Tail) if (strcmp(Name, E->Body.Leaf) == 0) break;
      break;
      case '0': case '1':
         Args = 0;
         H = Tag == '0'? 0: 1;
         E = ETab[H];
      break;
      case '+': case '*': case '?':
         Args = 1, X0 = va_arg(AP, Exp);
         H = X0->Hash;
         H = Tag == '+'? 0x02 + H*0x0a/0x200: Tag == '*'? 0x0c + H*0x14/0x200: 0x20 + H/0x10;
         for (E = ETab[H]; E != 0; E = E->Tail) if (X0 == E->Body.Arg[0]) break;
      break;
      case '|': case '.':
         Args = 2, X0 = va_arg(AP, Exp), X1 = va_arg(AP, Exp);
         H = Join(X0->Hash, X1->Hash);
         H = Tag == '|'? 0x40 + H/8: 0x80 + H/4;
         for (E = ETab[H]; E != 0; E = E->Tail) if (X0 == E->Body.Arg[0] && X1 == E->Body.Arg[1]) break;
      break;
   }
   va_end(AP);
   if (E == 0) {
      if (X == 0) {
         X = Allocate(sizeof *X);
         X->Hash = H, X->Q = -1, X->Mark = 0, X->Stack = 0;
      }
      X->Value = E = Allocate(sizeof *E);
      E->Tag = Tag, E->Class = X;
      if (Tag == 'x') E->Body.Leaf = CopyS(Name);
      else {
         E->Body.Arg = (Args > 0? Allocate(Args*sizeof *E->Body.Arg): 0);
         if (Args > 0) E->Body.Arg[0] = X0;
         if (Args > 1) E->Body.Arg[1] = X1;
      }
      E->Hash = H, E->Tail = ETab[H], ETab[H] = E;
   } else if (X != 0 && X != E->Class) X->Value = E, E->Class = X;
   return E->Class;
}

#define StackMax 0x80
char Stack[StackMax], *SP;
Var LStack[StackMax], *LSP;
Exp RStack[StackMax], *RSP;
void Enter(char Tag, ...) {
   static char *SEnd = Stack + sizeof Stack/sizeof *Stack;
   if (SP >= SEnd) Error("Expression too complex ... aborting."), exit(EXIT_FAILURE);
   va_list AP; va_start(AP, Tag);
   if (Tag == '$') SP = Stack, LSP = LStack, RSP = RStack;
   switch (*SP++ = Tag) {
      case '|': case '.': *RSP++ = va_arg(AP, Exp); break;
      case '=': *LSP++ = va_arg(AP, Var); break;
      case ',': *RSP++ = (*LSP)->Class, (*LSP++)->Class = va_arg(AP, Exp); break;
   }
   va_end(AP);
}

char Leave(char L) {
   unsigned P;
   switch (L) {
      default: P = 0; break;
      case '|': P = 1; break;
      case '=': P = 2; break;
      case ']': case ')': P = 3; break;
      case ',': P = 4; break;
      case '\0': P = 5; break;
   }
   char Tag = *--SP;
   switch (Tag) {
      case '.': if (P > 0) { RSP--; return Tag; } else break;
      case '|': if (P > 1) { RSP--; return Tag; } else break;
      case ',': if (P > 1) { LSP--, RSP--; return Tag; } else break;
      case '(': case '[': if (P > 2) return Tag; else break;
      case '=': if (P > 3) { LSP--; return Tag; } else break;
      default: if (P > 4) return Tag; else break;
   }
   SP++; return '\0';
}

Exp Parse(void) {
   Exp X; Var E; char Act;
   Enter('$');
   char L = Scan();
Start:
   switch (L) {
      case '(': case '[': Enter(L), L = Scan(); goto Start;
      case '0': case '1': X = MakeExp(0, L), L = Scan(); goto Reduce;
      case 'x':
         E = MakeVar(LastW);
         if ((L = Scan()) != '=') { X = E->Class; goto Reduce; }
         Enter('=', E), L = Scan();
      goto Start;
      default: Error("Corrupt expression."); return 0;
   }
Reduce:
   Act = Leave(L);
   switch (Act) {
      case '$': goto Accept;
      case '=':
         if (L == '\0') { Error("Missing operand after ','."); return 0; }
         else if (L != ',') Error("Missing ','."); else L = Scan();
         Enter(',', X);
      goto Start;
      case ',': (*LSP)->Class = *RSP; goto Reduce;
      case '(':
         if (L == ']') Error("( ... ]."), L = Scan();
         else if (L == ')') L = Scan(); else Error("Extra (.");
      goto Reduce;
      case '[':
         if (L == ')') Error("[ ... )."), L = Scan();
         else if (L == ']') L = Scan(); else Error("Extra [.");
         X = MakeExp(0, '?', X);
      goto Reduce;
      case '|': case '.': X = MakeExp(0, *SP, *RSP, X); goto Reduce;
   }
Shift:
   switch (L) {
      case ',': case ')': case ']': case '=': Error("Extra '%c'.", L); L = Scan(); goto Reduce;
      case '+': case '*': case '?': X = MakeExp(0, L, X), L = Scan(); goto Reduce;
      case '|': Enter('|', X), L = Scan(); goto Start;
      default: Enter('.', X); goto Start;
   }
Accept:
   return X;
}

Exp *QTab = 0; int Qs = 0, QMax = 0;
void AddState(Exp X) {
   if (X->Q >= 0) return;
   if (Qs == QMax) QMax += 0x10, QTab = Reallocate(QTab, sizeof *QTab * QMax);
   X->Q = Qs; QTab[Qs++] = X;
}

Exp *XStack = 0; int Xs = 0, XMax = 0;
void Push(Exp X) {
   if (X->Stack) return;
   if (Xs == XMax) XMax += 0x10, XStack = Reallocate(XStack, sizeof *XStack * XMax);
   XStack[Xs++] = X; X->Stack = 1;
}

void Pop(void) {
   Exp X = XStack[--Xs]; X->Stack = 0;
}

void FormState(Exp X) {
   AddState(X);
   for (int Q = 0; Q < Qs; Q++) {
      Push(QTab[Q]);
      while (Xs > 0) {
         X = XStack[Xs - 1];
         Var E = X->Value;
         if (X->Mark) { Pop(); continue; }
         switch (E->Tag) {
         // x = x 1, 0 and 1 are normal forms: mark, pop and (for x) add 1 as a state.
            case 'x': AddState(MakeExp(0, '1'));
            case '1': case '0': X->Mark = 1; Pop(); break;
         // A? => 1 | A
            case '?': MakeExp(X, '|', MakeExp(0, '1'), E->Body.Arg[0]); break;
         // A+ => A (1 | A+)
            case '+': MakeExp(X, '.', E->Body.Arg[0], MakeExp(0, '|', MakeExp(0, '1'), X)); break;
         // A* => 1 | A A*
            case '*': MakeExp(X, '|', MakeExp(0, '1'), MakeExp(0, '.', E->Body.Arg[0], X)); break;
         // A | B is a head normal form: mark and push A and B
            case '|': X->Mark = 1, Push(E->Body.Arg[0]), Push(E->Body.Arg[1]); break;
         // A B => (... based on the structure of A ...)
            case '.': {
               Var EA = E->Body.Arg[0]->Value; Exp XB = E->Body.Arg[1];
               switch (EA->Tag) {
               // x B is a normal form: mark, pop and add B as a state
                  case 'x': AddState(XB); X->Mark = 1; Pop(); break;
               // 1 B => B
                  case '1': X->Value = XB->Value; break;
               // 0 B => 0
                  case '0': MakeExp(X, '0'); break;
               // C? B => B | C B
                  case '?': MakeExp(X, '|', XB, MakeExp(0, '.', EA->Body.Arg[0], XB)); break;
               // C+ B => C (B | C+ B)
                  case '+': MakeExp(X, '.', EA->Body.Arg[0], MakeExp(0, '|', XB, X)); break;
               // C* B =>  B | C (C* B)
                  case '*': MakeExp(X, '|', XB, MakeExp(0, '.', EA->Body.Arg[0], X)); break;
               // (C | D) B => C B | D B
                  case '|': MakeExp(X, '|', MakeExp(0, '.', EA->Body.Arg[0], XB), MakeExp(0, '.', EA->Body.Arg[1], XB)); break;
               // (C D) B => C (D B)
                  case '.': MakeExp(X, '.', EA->Body.Arg[0], MakeExp(0, '.', EA->Body.Arg[1], XB)); break;
               }
            }
            break;
         }
      }
   }
}

void WriteStates(void) {
   for (int Q = 0; Q < Qs; Q++) {
      printf("Q%d =", Q), Push(QTab[Q]);
      for (int Xx = 0; Xx < Xs; Xx++) {
         Var E = XStack[Xx]->Value;
         switch (E->Tag) {
            case '0': break;
            case '1': printf(" 1"); break;
            case 'x': {
               Exp X = MakeExp(0, '1'); printf(" %s Q%d", E->Body.Leaf, X->Q);
            }
            break;
            case '|': Push(E->Body.Arg[0]), Push(E->Body.Arg[1]); break;
            case '.': {
               Var E0 = E->Body.Arg[0]->Value; Exp X1 = E->Body.Arg[1];
               printf(" %s Q%d", E0->Body.Leaf, X1->Q);
            }
            break;
         }
         if (E->Tag != '|' && E->Tag != '0' && Xx < Xs - 1) printf(" |");
      }
      while (Xs > 0) Pop();
      putchar('\n');
   }
}

int main(void) {
   Exp X = Parse();
   if (Errors > 0) fprintf(stderr, "%d error(s)\n", Errors);
   if (X == 0) return EXIT_FAILURE;
   FormState(X); WriteStates();
   return EXIT_SUCCESS;
}
