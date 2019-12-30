#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;

// Format:
//	STab[I][0] = the string's class
//	STab[I][1] = the string's length,
//	&STab[I][2] = the string
// I = 0, ..., Ss - 1, sorted by string length
static byte **STab;
static unsigned Ss, MaxQ;

void *Reallocate(void *X, size_t N) {
   X = X == 0? malloc(N): realloc(X, N);
   if (X == 0) fprintf(stderr, "Out of memory.\n"), exit(EXIT_FAILURE);
   return X;
}
#define Allocate(N) Reallocate(0, (N))

void Input(void) {
   static byte Buf[0x103]; byte *BP; int Diff, Lo, I, Ch;
   unsigned LINE;
   STab = 0, Ss = MaxQ = 0; LINE = 0;
   do {
      LINE++; BP = Buf + 1;
      while ((Ch = getchar()) != '\n' && Ch != EOF) {
         if (Ch == '\\') Ch = getchar();
         if (Ch == EOF) break;
         if (BP < Buf + sizeof Buf - 1) *BP++ = Ch;
      }
      if (BP <= Buf + 1)
         if (Ch == EOF) break; else continue;
      *BP = '\0'; *Buf = Buf[1]; Buf[1] = BP - Buf - 2;
      for (Diff = 1, Lo = 0; Lo < Ss; Lo++) {
         Diff = strcmp(STab[Lo] + 1, Buf + 1);
         if (Diff >= 0) break;
      }
      if (Diff == 0) {
         if (STab[Lo][0] != Buf[0])
            fprintf(stderr, "[%d] Duplicate definition: \"%s\".\n", LINE, Buf + 1), exit(EXIT_FAILURE);
         continue;
      }
      if (!(Ss&7)) STab = Reallocate(STab, (Ss + 8)*sizeof *STab);
      for (I = Ss++; I > Lo; I--) STab[I] = STab[I - 1];
      STab[Lo] = Allocate(BP - Buf + 1);
      *STab[Lo] = *Buf, strcpy(STab[Lo] + 1, Buf + 1);
      MaxQ += BP - Buf - 2;
   } while (Ch != EOF);
}

// Each state is a sum of the form:
//	Q = \Q + X1 (X1\Q) + ... + Xn (Xn\Q)
// with
//	Q.Unit = \Q
//	Q.Sum = { { X1, X1\Q }, .., { Xn, Xn\Q } }
//	Q.N = n
//	Q.Max = the current size limit of Q.Sum.
typedef struct { char X; unsigned Q; } *Term;
typedef struct {
   int Unit;
   unsigned N, Max; Term Sum;
} *State;
State QTab; unsigned Qs, QMax;
void AddState(void) {
   if (Qs >= QMax) QMax += 8, QTab = Reallocate(QTab, QMax*sizeof *QTab);
   QTab[Qs].Unit = -1, QTab[Qs].N = QTab[Qs].Max = 0, QTab[Qs].Sum = 0;
   Qs++;
}

void ShowStates(void) {
   static int Labels = 0;
   if (Qs < MaxQ) fprintf(stderr, "%d states...\n", Qs), MaxQ = Qs;
   printf("Solution %d, %d states\n", ++Labels, Qs);
   for (State Q = QTab; Q < QTab + Qs; Q++) {
      printf("Q%d =", Q - QTab);
      if (Q->Unit >= 0) {
         printf(" [%c]", Q->Unit);
         if (Q->N > 0) printf(" +"); else putchar('\n');
      }
      for (int T = 0; T < Q->N; T++) {
         printf(" %c Q%d", Q->Sum[T].X, Q->Sum[T].Q);
         if (T < Q->N - 1) printf(" +"); else putchar('\n');
      }
   }
}

// Format:
//	ITab[I] holds the suspended search items, I = 0 ... Is - 1
//	IMax is the current size limit of ITab
typedef struct Item *Item;
struct Item {
   byte **Str, *Pos; unsigned Q; byte End;
// End == 0 -> *Pos\Q = Delta(*Pos, Q), End == 1 -> \Q = QTab[Q].Unit
};
Item ITab; int Is, IMax;

// Str is the search pointer into STab, Pos points into Str's string.
byte **Str, *Pos; unsigned Q;
void Save(byte End) {
   if (Is >= IMax) IMax += 8, ITab = Reallocate(ITab, IMax*sizeof *ITab);
   ITab[Is].Str = Str, ITab[Is].Pos = Pos, ITab[Is].Q = Q, ITab[Is].End = End;
   Is++;
}

unsigned Delta(char X, unsigned Q) {
   State S = &QTab[Q]; int T = 0;
   for (; T < S->N; T++) if (S->Sum[T].X == X) return S->Sum[T].Q;
   if (S->N >= S->Max) S->Max += 4, S->Sum = Reallocate(S->Sum, S->Max*sizeof *S->Sum);
   Save(0);
   S->Sum[T].X = X, S->Sum[T].Q = 0, S->N++;
   return 0;
}

int Update(void) {
   char X = *Pos; State S = &QTab[Q]; int T = 0;
   for (; T < S->N; T++) if (S->Sum[T].X == X) break;
   if (T >= S->N) return 0;
   unsigned Q1 = S->Sum[T].Q;
   if (Q1 >= Qs - 1 && QTab[Q1].N == 0 && QTab[Q1].Unit < 0) Qs--;
   if (++Q1 > Qs || Q1 >= MaxQ || Qs > MaxQ) {
      S->N--, S->Sum[T].X = S->Sum[S->N].X, S->Sum[T].Q = S->Sum[S->N].Q;
      return 0;
   }
   if (Q1 >= Qs) AddState();
   S->Sum[T].Q = Q1; Save(0); return 1;
}

// Search algorithms tend to be small code-wise, like this one. But it's not efficient.
void Search(void) {
   fprintf(stderr, "Searching...\n");
   QTab = 0, Qs = QMax = 0; AddState();
   Str = STab, ITab = 0, Is = IMax = 0;
Search:
   if (Str >= STab + Ss) goto BackTrack;
   Pos = &(*Str)[2], Q = 0;
Next:
   for (; Pos < &(*Str)[2] + (*Str)[1]; Pos++) Q = Delta(*Pos, Q);
   if (QTab[Q].Unit < 0) QTab[Q].Unit = (*Str)[0], Save(1);
   if (QTab[Q].Unit == (*Str)[0]) { Str++; goto Search; }
BackTrack:
   if (Str - STab >= Ss) ShowStates();
   while (Is-- > 0) {
      Str = ITab[Is].Str, Pos = ITab[Is].Pos, Q = ITab[Is].Q;
      if (ITab[Is].End) QTab[Q].Unit = -1; else if (Update()) break;
   }
   if (Is > 0) goto Next;
}

int main(void) {
   Input(); Search();
   return EXIT_SUCCESS;
}
