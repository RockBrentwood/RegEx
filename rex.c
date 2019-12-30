#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#define REX_VERSION "rex version 1.2"

#define SYS_DOS 0
#define SYS_WIN 1
#define SYS_NIX 2
#define SYSTEM SYS_NIX

// Derived from the syntax:
// Ex = Set | "(" Ex ")" | Ex "+" | Ex "*" | Ex "?" | Ex Ex | Ex "|" Ex | Ex "^" Ex | Ex "-" Ex | Ex "&" Ex.
// with +, *, ? binding more tightly than concatenation, which binds more tightly than &, ^, which bind more tightly than |, -.

// The Lexical Scanner.
typedef unsigned char byte;

#define isspecial(X) ((X) >= 0x80)
const byte BegLine = 0x80, EndLine = 0x81, BegWord = 0x82, EndWord = 0x83;

static char *Ex, *Fl; FILE *ExF;

static int DoI;
static byte Get(void) {
   int Ch;
   if (ExF != NULL) {
      Ch = fgetc(ExF);
      if (Ch == '\n') {
         int Next = fgetc(ExF);
         if (Next != EOF) Ch = '|', ungetc(Next, ExF); else Ch = EOF;
      }
      if (Ch == EOF) fclose(ExF), ExF = NULL, Ch = '\0';
   } else {
      if (*Ex == '\0') Ex = NULL;
      if (Ex == NULL) Ch = '\0';
      else Ch = *Ex++;
   }
   if (DoI && isupper(Ch)) Ch = tolower(Ch);
   return ((byte)Ch)&0x7f;
}

static void Fatal(void) { fprintf(stderr, "Syntax error.\n"), exit(EXIT_FAILURE); }

typedef struct Exp *Exp;
typedef struct Set *Set;
#define SetElems 0x88
#define SetSize 0x11
// const size_t SetElems = 0x88, SetSize = 0x11;
// Room for ASCII characters plus up to 8 special characters.
struct Set {
   byte X[SetSize]; Exp Pos, Neg;
};

void *Allocate(size_t N) {
   void *X = calloc(N, 1);
   if (X == NULL) fprintf(stderr, "Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

void *Reallocate(void *X, size_t N) {
   X = realloc(X, N);
   if (X == NULL) fprintf(stderr, "Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}

static Set SetTab = NULL; static unsigned Sets = 0;
static byte Buf[SetSize];
static int EmptySet = 0, Special = 0;

int FindS(void) {
   for (int X = 0; X < Sets; X++) {
      byte *A = SetTab[X].X;
      if (A[0] == Buf[0]) {
         int I = 1;
         for (; A[I] == Buf[I] && I < SetSize; I++);
         if (I >= SetSize) return ++X;
      } else if ((A[0]^Buf[0]) == 0xff) {
         int I = 0;
         for (; (A[I]^Buf[I]) == 0xff && I < SetSize; I++);
         if (I >= SetSize) return -(++X);
      }
   }
   if (!(Sets&7)) SetTab = Reallocate(SetTab, (Sets + 8)*sizeof *SetTab);
   byte *A = SetTab[Sets].X; SetTab[Sets].Pos = SetTab[Sets].Neg = 0;
   for (int I = 0; I < SetSize; I++) A[I] = Buf[I];
   return ++Sets;
}

int SetVal;
char Scan(void) {
   byte Ch = Get();
   switch (Ch) {
      case '\0': return 0;
      case '|': case '-': case '&': case '^': case '(': case ')': case '*': case '+': case '?': return Ch;
      case '.': SetVal = -EmptySet; return 'x';
      case '[': {
         for (int I = 0; I < SetSize; I++) Buf[I] = 0;
         byte Neg = 0;
         if ((Ch = Get()) == '^') Neg = 1, Ch = Get();
         while (Ch != ']' && Ch != '\0') {
            int Beg = Ch, End;
            if ((Ch = Get()) != '-') End = Beg;
            else {
               if ((Ch = Get()) == 0) Fatal();
               End = Ch, Ch = Get();
            }
            for (int I = Beg; I <= End; I++) Buf[I >> 3] |= 1 << (I&7);
         }
         if (Ch == '\0') Fatal();
         SetVal = FindS(); if (Neg) SetVal = -SetVal;
      }
      return 'x';
      case '{': return '{';
      case '}': return '}';
      case '<': Ch = BegWord; goto Character;
      case '>': Ch = EndWord; goto Character;
      case '\\':
         if ((Ch = Get()) == '\0') return 0;
      default: Character:
         for (int I = 0; I < SetSize; I++) Buf[I] = 0;
         Buf[Ch >> 3] = 1 << (Ch&7);
         SetVal = FindS();
      return 'x';
   }
}

int Intersect(int A, int B) {
   int nA = A < 0; if (nA) A = -A; Set sA = &SetTab[--A];
   int nB = B < 0; if (nB) B = -B; Set sB = &SetTab[--B];
   for (int I = 0; I < SetSize; I++) Buf[I] = (nA? sA->X[I]^0xff: sA->X[I])&(nB? sB->X[I]^0xff: sB->X[I]);
   return FindS();
}

typedef struct Term { int X; Exp Q; } *Term;
struct Exp {
   char Tag; unsigned ID; int Hash; Exp Tail;
   int Terms; Term Sum; byte Unit; Exp *Del;
   Exp Arg[2];
};

static Exp Zero, Unit, All, BegEx, EndEx;
static Exp ExpHash[0x100];

Exp GetExp(char Tag, ...) {
   static unsigned long ID = 2;
   int H, Div; Exp E, E0, E1;
   va_list AP; va_start(AP, Tag);
   switch (Tag) {
      case '0': E = Zero; break;
      case '1': E = Unit; break;
      case 'x': {
         int S = va_arg(AP, int);
         Exp *EP = S < 0? &SetTab[-S - 1].Neg: &SetTab[S - 1].Pos;
         E = *EP;
         if (E == NULL) {
            *EP = E = Allocate(sizeof *E); E->ID = ID++;
            E->Tag = 'x';
            E->Unit = 0; E->Del = NULL;
            E->Terms = 3, E->Sum = Allocate(E->Terms*sizeof *E->Sum);
            E->Sum[0].X = S, E->Sum[0].Q = Unit;
         // Special characters (word boundaries) are made transparent.
            E->Sum[1].X = Special, E->Sum[1].Q = E;
            E->Sum[2].X = -S, E->Sum[2].Q = Zero;
            if (S < 0) S = -S; S--; E->Hash = S%0x100; E->Tail = NULL;
         }
      }
      break;
      case '+': H = 0x00, Div = 8; goto MakeUnary;
      case '*': H = 0x20, Div = 8; goto MakeUnary;
      case '?': H = 0x40, Div = 0x10; goto MakeUnary;
      MakeUnary:
         E0 = va_arg(AP, Exp), H += E0->Hash/Div;
         for (E = ExpHash[H]; E != NULL; E = E->Tail) if (E0 == E->Arg[0]) break;
      break;
      case '^': H = 0x50, Div = 0x10; goto MakeBinary;
      case '&': H = 0x60, Div = 0x10; goto MakeBinary;
      case '-': H = 0x70, Div = 0x10; goto MakeBinary;
      case '|': H = 0x80, Div = 4; goto MakeBinary;
      case '.': H = 0xc0, Div = 4; goto MakeBinary;
      MakeBinary:
         E0 = va_arg(AP, Exp), E1 = va_arg(AP, Exp), H += (E0->Hash^E1->Hash)/Div;
         for (E = ExpHash[H]; E != NULL; E = E->Tail) if (E0 == E->Arg[0] && E1 == E->Arg[1]) break;
      break;
   }
   va_end(AP);
   if (E == NULL) {
      E = Allocate(sizeof *E); E->Tag = Tag, E->ID = ID++;
      E->Terms = 0, E->Sum = NULL;
      E->Arg[0] = E0;
      E->Hash = H, E->Tail = ExpHash[H], ExpHash[H] = E;
      E->Del = NULL;
      switch (Tag) {
         case '+': E->Unit = E0->Unit; break;
         case '*': case '?': E->Unit = 1; break;
         case '^': case '&': case '.': E->Arg[1] = E1; E->Unit = E0->Unit && E1->Unit; break;
         case '-': E->Arg[1] = E1; E->Unit = E0->Unit && !E1->Unit; break;
         case '|': E->Arg[1] = E1; E->Unit = E0->Unit || E1->Unit; break;
      }
   }
   return E;
}

Exp MakeChar(byte B) {
   for (int I = 0; I < SetSize; I++) Buf[I] = 0;
   Buf[B >> 3] |= 1 << (B&7);
   return GetExp('x', FindS());
}

static Exp *AStack = NULL, *BStack = NULL;

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
   for (As = 0; A->Tag == '.'; A = A->Arg[1]) InsertA(A->Arg[0]);
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
   for (As = 0; A->Tag == Tag; A = A->Arg[1]) InsertA(A->Arg[0]);
   for (Bs = 0; B->Tag == Tag; B = B->Arg[1]) InsertB(B->Arg[0]);
   Exp C;
   if (A->ID > B->ID) C = A, InsertB(B); else C = B, InsertA(A);
   As--, Bs--;
   while (As >= 0 || Bs >= 0) {
      int Diff = 0;
      if (As >= 0) A = AStack[As]; else Diff = -1;
      if (Bs >= 0) B = BStack[Bs]; else Diff = +1;
      if (Diff == 0) Diff = A->ID - B->ID;
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
         case '^': // 1 ^ x = x = x ^ 1, x ^ 0 = 0 = 0 ^ x
            if (D->Tag == '0' || C->Tag == '1') { C = D; continue; }
            if (D->Tag == '1' || C->Tag == '0') continue;
         break;
      }
      C = GetExp(Tag, D, C);
   }
   return C;
}

void InitExp(void) {
   for (int I = 0; I < SetSize; I++) Buf[I] = 0;
   if (EmptySet == 0) EmptySet = FindS();
   if (Special == 0) Buf[BegWord >> 3] |= 1 << (BegWord&7), Buf[EndWord >> 3] |= 1 << (EndWord&7), Special = FindS();
   if (Zero == NULL) {
      Zero = Allocate(sizeof *Zero), Zero->Tag = '0', Zero->ID = 0;
      Zero->Hash = 0, Zero->Tail = NULL, Zero->Unit = 0; Zero->Del = NULL;
      Zero->Terms = 1, Zero->Sum = Allocate(Zero->Terms*sizeof *Zero->Sum);
      Zero->Sum[0].X = -EmptySet, Zero->Sum[0].Q = Zero;
   }
   if (Unit == NULL) {
      Unit = Allocate(sizeof *Unit), Unit->Tag = '1', Unit->ID = 1;
      Unit->Hash = 1, Unit->Tail = NULL, Unit->Unit = 1; Unit->Del = NULL;
      Unit->Terms = 1, Unit->Sum = Allocate(Unit->Terms*sizeof *Unit->Sum);
      Unit->Sum[0].X = -EmptySet, Unit->Sum[0].Q = Zero;
   }
   if (All == NULL) All = GetExp('*', GetExp('x', -EmptySet));
   if (BegEx == NULL) BegEx = MakeChar(BegLine);
   if (EndEx == NULL) EndEx = MakeChar(EndLine);
}

// #define StackMax 100
#define StackMax 100
// const size_t StackMax = 100;
char Stack[StackMax], *SP = Stack;
Exp VStack[StackMax], *VSP = VStack;
static void Enter(char Tag, ...) {
   static char *SEnd = Stack + sizeof Stack/sizeof *Stack;
   if (SP >= SEnd) Fatal();
   va_list AP; va_start(AP, Tag);
   switch (*SP++ = Tag) {
      case '^': case '&': case '-': case '|': case '.': *VSP++ = va_arg(AP, Exp); break;
   }
   va_end(AP);
}

static char Leave(char L) {
   unsigned P;
   switch (L) {
      default: P = 0; break;
      case '&': case '^': P = 1; break;
      case '|': case '-': P = 2; break;
      case ')': P = 4; break;
      case 0: P = 5; break;
   }
   char Tag = *--SP;
   switch (Tag) {
      case '^': case '&': case '.': if (P > 0) { VSP--; return Tag; } break;
      case '|': case '-': if (P > 1) { VSP--; return Tag; } break;
      case '(': if (P > 2) return Tag; break;
      default: if (P > 3) return Tag; break;
   }
   SP++; return '\0';
}

Exp Parse(void) {
   char Act; Exp E;
   Enter('$');
   char L = Scan();
Start:
   switch (L) {
      case '(': Enter('('); L = Scan(); goto Start;
      case '{': E = BegEx; L = Scan(); goto Reduce;
      case '}': E = EndEx; L = Scan(); goto Reduce;
      case 'x': E = GetExp('x', SetVal); L = Scan(); goto Reduce;
      default: Fatal();
   }
Reduce:
   switch (Act = Leave(L)) {
      case '$': if (L != 0) Fatal(); goto End;
      case '(': if (L == ')') L = Scan(); else Fatal(); goto Reduce;
      case '|': case '^': case '&': E = BinExp(Act, *VSP, E); goto Reduce;
      case '-': E = GetExp('-', *VSP, E); goto Reduce;
      case '.': E = CatExp(*VSP, E); goto Reduce;
   }
Shift:
   switch (L) {
      default: Enter('.', E); goto Start;
      case '|': case '^': case '&': case '-': Enter(L, E); L = Scan(); goto Start;
      case '+': case '*': case '?': E = GetExp(L, E); L = Scan(); goto Reduce;
   }
End:
   return E;
}

typedef struct XItem *XItem;
struct XItem { byte Arg; Exp E; Term dA; };
static XItem XStack;
static unsigned Xs, XMax = 0;
void PushX(byte Arg, Exp E, Term dA) {
   if (Xs >= XMax) XMax += 8, XStack = Reallocate(XStack, XMax*sizeof *XStack);
   XItem X = &XStack[Xs++];
   X->Arg = Arg, X->E = E, X->dA = dA;
}

// This branch approaches 100% certainty in very short order.
#define Delta(E, X) ((E)->Del != NULL && (E)->Del[X] != NULL? (E)->Del[X]: _Delta((E), (X)))
Exp _Delta(Exp E, byte X) {
   int T;
   Xs = 0;
Start:
   while (1) {
      for (T = 0; T < E->Terms; T++) {
         int S = E->Sum[T].X;
         int Neg = S < 0; if (Neg) S = -S;
         int Y = SetTab[--S].X[X >> 3]&(1 << (X&7));
         if (Neg? !Y: Y) goto End;
      }
      if (!(E->Terms&3)) E->Sum = Reallocate(E->Sum, (E->Terms + 4)*sizeof *E->Sum);
      E->Terms++;
      PushX(0, E, 0); E = E->Arg[0];
   }
End:
   while (Xs-- > 0) {
      Term dE = E->Sum + T;
      E = XStack[Xs].E, T = E->Terms - 1;
      if (XStack[Xs].Arg) {
         Term dA = XStack[Xs].dA, dB = dE;
         E->Sum[T].X = Intersect(dA->X, dB->X);
         switch (E->Tag) {
            case '.': // x\ab = (x\a) b | \a x\b = (x\a) b | x\b, if \a == 1
               E->Sum[T].Q = BinExp('|', CatExp(dA->Q, E->Arg[1]), dB->Q);
            break;
            default: // x\(a op b) = x\a op x\b
               E->Sum[T].Q = BinExp(E->Tag, dA->Q, dB->Q);
            break;
            case '^': // x\(a ^ b) = x\a ^ b | a ^ x\b
               E->Sum[T].Q = BinExp('|', BinExp('^', dA->Q, E->Arg[1]), BinExp('^', E->Arg[0], dB->Q));
            break;
         }
      } else {
         Term dA = dE;
         switch (E->Tag) {
            case '?': // x\a? = x\a
               E->Sum[T].X = dA->X, E->Sum[T].Q = dA->Q;
            break;
            case '*': case '+': // x\a* = (x\a) a* = x\a+
               E->Sum[T].X = dA->X;
               E->Sum[T].Q = CatExp(dA->Q, E->Tag == '+' && !E->Unit? GetExp('*', E->Arg[0]): E);
            break;
            default:
               if (E->Tag != '.' || E->Arg[0]->Unit) {
                  PushX(1, E, dA), E = E->Arg[1]; goto Start;
               }
            // x\ab = (x\a) b | \a x\b = (x\a) b, if \a == 0
               E->Sum[T].X = dA->X, E->Sum[T].Q = CatExp(dA->Q, E->Arg[1]);
            break;
         }
      }
   }
   if (E->Del == NULL) E->Del = Allocate(SetElems*sizeof *E->Del);
   for (int Y = 0; Y < E->Terms; Y++) {
      int S = E->Sum[Y].X; Exp Q = E->Sum[Y].Q;
      int Neg = S < 0; if (Neg) S = -S;
      Set St = &SetTab[--S];
      for (int X = 0; X < SetElems; X++)
         if (E->Del[X] == NULL && (St->X[X >> 3]&(1 << (X&7))? !Neg: Neg)) E->Del[X] = Q;
   }
   return E->Sum[T].Q;
}

// #define LineMax 0x100
#define LineMax 0x100
// const size_t LineMax = 0x100;
typedef struct Line *Line;
struct Line {
   byte Match; long Offset; byte Buf[LineMax + 1];
};
static Line LTab;
static unsigned Lines = 0;
static int Af = 0, Bf = 0;
static int DoB, DoC, DoN, CurLine;
static int TotalMatches;
static int DoH, DoL;

int Match(char *File, Exp Q0) {
   if (Q0 == NULL) return 1;
// An empty expression is treated as matching nothing.
// This is used to count files in a first pass before matching them against an actual expression in the second pass.
   FILE *InF;
   if (File == NULL) InF = stdin;
   else {
      InF = fopen(File, "r"); if (InF == NULL) { fprintf(stderr, "Cannot open %s.\n", File); return 0; }
      if (setvbuf(InF, 0, _IOFBF, 0x1000) != 0) { fprintf(stderr, "Error opening %s.\n", File); return 0; }
   }
   for (int L = 0; L < Lines; L++) LTab[L].Match = 0, LTab[L].Offset = 0L, *LTab[L].Buf = '\0';
   int Ls = CurLine = 0;
   int EndFile = 0, After = 0;
   int Follow = 0, LineX = -Bf, Match = 0;
   int Matches = 0; // Not sure if this should be initialized to 0 here.
   Exp Q; int InWord; byte *CurL; int Ch;
StartLine:
   Q = Delta(Q0, BegLine), InWord = 0;
   if (DoB) LTab[Ls].Offset = ftell(InF);
   CurL = LTab[Ls].Buf;
   while ((Ch = fgetc(InF)) != '\n' && Ch != EOF) {
   // Word boundaries are treated as transparent.
      if (InWord && ispunct(Ch) || isspace(Ch)) Q = Delta(Q, EndWord), InWord = 0;
      else if (!InWord && !ispunct(Ch) && !isspace(Ch)) Q = Delta(Q, BegWord), InWord = 1;
      if (CurL < LTab[Ls].Buf + LineMax) *CurL++ = (byte)Ch;
      if (DoI && isupper(Ch)) Ch = tolower(Ch);
      Q = Delta(Q, (byte)Ch);
   }
   if (Ch == EOF && CurL == LTab[Ls].Buf) { EndFile = 1; After = 0; goto CheckLine; }
   *CurL++ = 0;
   if (InWord) Q = Delta(Q, EndWord), InWord = 0;
   Q = Delta(Q, EndLine);
   if (Q->Unit) {
      if (DoL) { fprintf(stderr, "%s\n", File); return 1; }
      int J;
      if (LineX < 0) J = -1; else J = LineX, LTab[J].Match = 1;
      while (1) {
         if (++J >= Lines) J = 0;
         if (J == Ls) break;
         LTab[J].Match = 1;
      }
      Follow = Af + 1;
   }
   LTab[Ls].Match = Follow > 0;
   if (++Ls >= Lines) Ls = 0;
CheckLine:
   if (Follow > 0) Follow--;
   if (EndFile && After++ >= Af) { fclose(InF), InF = NULL; return Matches; }
   if (LineX >= 0) {
      if (LTab[LineX].Match) {
         if (!Match && TotalMatches > 0 && Af + Bf > 0) printf("-----\n");
         TotalMatches++, Matches++;
         if (!DoH) printf("%s:", File);
         if (DoB) printf("%ld:", LTab[LineX].Offset), LTab[LineX].Offset = 0L;
         if (DoN) printf("%d:", CurLine + 1 - Bf);
         printf("%s\n", LTab[LineX].Buf);
      }
      Match = LTab[LineX].Match;
   }
   if (++LineX >= (int)Lines) LineX = 0;
   CurLine++;
   if (EndFile) goto CheckLine;
goto StartLine;
}

// Support for wild-cards.
#if SYSTEM == SYS_DOS
#include <io.h>
#include <dos.h>
int MatchCount(char *Patt, Exp E) {
   struct find_t Find;
   if (_dos_findfirst(P, _A_ARCH, &Find) != 0) { printf("%s: not found.\n", P); return 0; }
   int I = 0;
   do I += Match(Find.name, E); while (_dos_findnext(&Match) == 0);
   return I;
}
#elif SYSTEM == SYS_WIN
#include <io.h>
int MatchCount(char *Patt, Exp E) {
   struct _finddata_t Find;
   long FindH = _findfirst(Patt, &Find); if (FindH == 1) { printf("%s: not found.\n", Patt); return 0; }
   int I = 0;
   do I += Match(Find.name, E); while (_findnext(FindH, &Find) == 0);
   _findclose(FindH);
   return I;
}
#elif SYSTEM == SYS_NIX
int MatchCount(char *Patt, Exp E) { return Match(Patt, E); }
#else
#   error "Unrecognized system type"
#endif

int main(int AC, char **AV) {
// assert(SetElems == 8*SetSize);
   int DoS = 0, DoV = 0, DoW = 0, DoX = 0;
   DoB = DoC = DoH = DoL = DoN = DoI = 0;
   Ex = Fl = NULL;
   char *App = AV[0]; if (App == NULL || *App == '\0') App = "rex";
   int Arg = 1;
   for (; Arg < AC; Arg++) {
      char *AP = AV[Arg];
      if (*AP++ != '-') break;
      for (; *AP != '\0'; AP++) switch (*AP) {
         case 'V': fprintf(stderr, "%s\n", REX_VERSION); return EXIT_SUCCESS;
         case 's': DoS++; break;
         case 'v': DoV++; break;
         case 'w': DoW++; break;
         case 'x': DoX++; break;
         case 'b': DoB++; break;
         case 'c': DoC++; break;
         case 'h': DoH++; break;
         case 'l': DoL++; break;
         case 'n': DoN++; break;
         case 'i': DoI++; break;
         case 'C': Af = Bf = 2; break;
         case 'e': case 'f': case 'A': case 'B': {
            char Mode = *AP++;
            if (*AP == '\0')
               if (++Arg >= AC) goto Help; else AP = AV[Arg];
            switch (Mode) {
               case 'A': Af = atoi(AP); if (Af <= 0) goto Help; break;
               case 'B': Bf = atoi(AP); if (Bf <= 0) goto Help; break;
               case 'e': Ex = AP; break;
               default: Fl = AP; break;
            }
         }
         goto NextWord;
         default:
            if (!isdigit(*AP)) goto Help;
            Af = Bf = atoi(AP);
         goto NextWord;
      }
   NextWord: ;
   }
   if (Ex == NULL && Fl == NULL) {
      if (Arg >= AC) goto Help;
      Ex = AV[Arg++];
   }
   if (Ex != NULL && Fl != NULL) goto Help;
   if (Fl == NULL) ExF = NULL;
   else {
      ExF = fopen(Fl, "r");
      if (ExF == NULL) fprintf(stderr, "Cannot open %s.\n", Fl), exit(EXIT_FAILURE);
   }
   InitExp();
   Exp RE = Parse();
   if (DoL || DoC) DoB = 0, Af = Bf = 0;
   if (DoL) DoC = 0;
   Lines = 1 + Bf, LTab = Allocate(Lines*sizeof *LTab);
// -w: E = <E>
   if (DoW) RE = CatExp(MakeChar(BegWord), CatExp(RE, MakeChar(EndWord)));
// -x: E = {<?E>?}; else default: E = X E X
   if (DoX) RE = CatExp(BegEx, CatExp(RE, EndEx));
   else RE = CatExp(All, CatExp(RE, All));
// -v: E = X - E
   if (DoV) RE = GetExp('-', All, RE);
// -s: Parse only; no actual matching.
   if (DoS) return EXIT_SUCCESS;
   TotalMatches = 0;
   if (Arg >= AC) {
      DoH = 1;
      if (DoL) { fprintf(stderr, "File inputs required with -l option.\n"); return EXIT_FAILURE; }
      Match(0, RE);
   } else if (AC - Arg <= 1) {
      int Files = MatchCount(AV[Arg], 0);
      if (Files == 1 && !DoH) DoH = 1;
      if (Files >= 1) MatchCount(AV[Arg], RE);
   } else
      for (; Arg < AC; Arg++) MatchCount(AV[Arg], RE);
   if (DoC) printf("%d matched line(s).\n", TotalMatches);
   return TotalMatches > 0? EXIT_SUCCESS: EXIT_FAILURE;
Help:
   printf("Usage: %s -[bchlnivwxC] -e Exp -f File -[AB]# -# Exp File...\n", App);
   return EXIT_FAILURE;
}
