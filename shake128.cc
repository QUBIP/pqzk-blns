/*
Copyright (c) 2022-2024 IBM

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
 * Based on the public domain implementation in
 * crypto_hash/keccakc512/simple/ from http://bench.cr.yp.to/supercop.html
 * by Ronny Van Keer and the public domain "TweetFips202" implementation
 * from https://twitter.com/tweetfips202 by Gilles Van Assche,
 * Daniel J. Bernstein, and Peter Schwabe
 */

#include "shake128.h"

#define SHAKE128_RATE 168
#define NROUNDS 24
#define ROL(a, offset) ((a << offset) ^ (a >> (64 - offset)))

static void KeccakF1600_StatePermute (uint64_t state[25]);
static void keccak_init (shake128_state_t *state);
static unsigned int keccak_absorb (uint64_t s[25], unsigned int r,
                                   unsigned int pos, const uint8_t *m,
                                   size_t mlen);
static void keccak_finalize (uint64_t s[25], unsigned int r, unsigned int pos,
                             uint8_t p);
static unsigned int keccak_squeeze (uint8_t *out, size_t outlen,
                                    uint64_t s[25], unsigned int r,
                                    unsigned int pos);
void
_shake128_init (shake128_state_t *state)
{
  keccak_init (state);
}

void
_shake128_absorb (shake128_state_t *state, const uint8_t *in, size_t len)
{


  state->pos = keccak_absorb (state->s, SHAKE128_RATE, state->pos, in, len);
}

void
_shake128_squeeze (shake128_state_t *state, uint8_t *out, size_t len)
{
  if (state->final == 0)
    {
      keccak_finalize (state->s, SHAKE128_RATE, state->pos, 0x1F);
      state->pos = 0;
      state->final = 1;
    }
  state->pos = keccak_squeeze (out, len, state->s, SHAKE128_RATE, state->pos);
}


static uint64_t
load64 (const uint8_t x[8])
{
  unsigned int i;
  uint64_t r = 0;

  for (i = 0; i < 8; i++)
    r |= (uint64_t)x[i] << 8 * i;

  return r;
}

static void
store64 (uint8_t x[8], uint64_t u)
{
  unsigned int i;

  for (i = 0; i < 8; i++)
    x[i] = u >> 8 * i;
}

/* Keccak round constants */
static const uint64_t KeccakF_RoundConstants[NROUNDS]
    = { (uint64_t)0x0000000000000001ULL, (uint64_t)0x0000000000008082ULL,
        (uint64_t)0x800000000000808aULL, (uint64_t)0x8000000080008000ULL,
        (uint64_t)0x000000000000808bULL, (uint64_t)0x0000000080000001ULL,
        (uint64_t)0x8000000080008081ULL, (uint64_t)0x8000000000008009ULL,
        (uint64_t)0x000000000000008aULL, (uint64_t)0x0000000000000088ULL,
        (uint64_t)0x0000000080008009ULL, (uint64_t)0x000000008000000aULL,
        (uint64_t)0x000000008000808bULL, (uint64_t)0x800000000000008bULL,
        (uint64_t)0x8000000000008089ULL, (uint64_t)0x8000000000008003ULL,
        (uint64_t)0x8000000000008002ULL, (uint64_t)0x8000000000000080ULL,
        (uint64_t)0x000000000000800aULL, (uint64_t)0x800000008000000aULL,
        (uint64_t)0x8000000080008081ULL, (uint64_t)0x8000000000008080ULL,
        (uint64_t)0x0000000080000001ULL, (uint64_t)0x8000000080008008ULL };

static void
KeccakF1600_StatePermute (uint64_t state[25])
{
  int round;

  uint64_t Aba, Abe, Abi, Abo, Abu;
  uint64_t Aga, Age, Agi, Ago, Agu;
  uint64_t Aka, Ake, Aki, Ako, Aku;
  uint64_t Ama, Ame, Ami, Amo, Amu;
  uint64_t Asa, Ase, Asi, Aso, Asu;
  uint64_t BCa, BCe, BCi, BCo, BCu;
  uint64_t Da, De, Di, Do, Du;
  uint64_t Eba, Ebe, Ebi, Ebo, Ebu;
  uint64_t Ega, Ege, Egi, Ego, Egu;
  uint64_t Eka, Eke, Eki, Eko, Eku;
  uint64_t Ema, Eme, Emi, Emo, Emu;
  uint64_t Esa, Ese, Esi, Eso, Esu;

  // copyFromState(A, state)
  Aba = state[0];
  Abe = state[1];
  Abi = state[2];
  Abo = state[3];
  Abu = state[4];
  Aga = state[5];
  Age = state[6];
  Agi = state[7];
  Ago = state[8];
  Agu = state[9];
  Aka = state[10];
  Ake = state[11];
  Aki = state[12];
  Ako = state[13];
  Aku = state[14];
  Ama = state[15];
  Ame = state[16];
  Ami = state[17];
  Amo = state[18];
  Amu = state[19];
  Asa = state[20];
  Ase = state[21];
  Asi = state[22];
  Aso = state[23];
  Asu = state[24];

  for (round = 0; round < NROUNDS; round += 2)
    {
      //    prepareTheta
      BCa = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
      BCe = Abe ^ Age ^ Ake ^ Ame ^ Ase;
      BCi = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
      BCo = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
      BCu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;

      // thetaRhoPiChiIotaPrepareTheta(round  , A, E)
      Da = BCu ^ ROL (BCe, 1);
      De = BCa ^ ROL (BCi, 1);
      Di = BCe ^ ROL (BCo, 1);
      Do = BCi ^ ROL (BCu, 1);
      Du = BCo ^ ROL (BCa, 1);

      Aba ^= Da;
      BCa = Aba;
      Age ^= De;
      BCe = ROL (Age, 44);
      Aki ^= Di;
      BCi = ROL (Aki, 43);
      Amo ^= Do;
      BCo = ROL (Amo, 21);
      Asu ^= Du;
      BCu = ROL (Asu, 14);
      Eba = BCa ^ ((~BCe) & BCi);
      Eba ^= (uint64_t)KeccakF_RoundConstants[round];
      Ebe = BCe ^ ((~BCi) & BCo);
      Ebi = BCi ^ ((~BCo) & BCu);
      Ebo = BCo ^ ((~BCu) & BCa);
      Ebu = BCu ^ ((~BCa) & BCe);

      Abo ^= Do;
      BCa = ROL (Abo, 28);
      Agu ^= Du;
      BCe = ROL (Agu, 20);
      Aka ^= Da;
      BCi = ROL (Aka, 3);
      Ame ^= De;
      BCo = ROL (Ame, 45);
      Asi ^= Di;
      BCu = ROL (Asi, 61);
      Ega = BCa ^ ((~BCe) & BCi);
      Ege = BCe ^ ((~BCi) & BCo);
      Egi = BCi ^ ((~BCo) & BCu);
      Ego = BCo ^ ((~BCu) & BCa);
      Egu = BCu ^ ((~BCa) & BCe);

      Abe ^= De;
      BCa = ROL (Abe, 1);
      Agi ^= Di;
      BCe = ROL (Agi, 6);
      Ako ^= Do;
      BCi = ROL (Ako, 25);
      Amu ^= Du;
      BCo = ROL (Amu, 8);
      Asa ^= Da;
      BCu = ROL (Asa, 18);
      Eka = BCa ^ ((~BCe) & BCi);
      Eke = BCe ^ ((~BCi) & BCo);
      Eki = BCi ^ ((~BCo) & BCu);
      Eko = BCo ^ ((~BCu) & BCa);
      Eku = BCu ^ ((~BCa) & BCe);

      Abu ^= Du;
      BCa = ROL (Abu, 27);
      Aga ^= Da;
      BCe = ROL (Aga, 36);
      Ake ^= De;
      BCi = ROL (Ake, 10);
      Ami ^= Di;
      BCo = ROL (Ami, 15);
      Aso ^= Do;
      BCu = ROL (Aso, 56);
      Ema = BCa ^ ((~BCe) & BCi);
      Eme = BCe ^ ((~BCi) & BCo);
      Emi = BCi ^ ((~BCo) & BCu);
      Emo = BCo ^ ((~BCu) & BCa);
      Emu = BCu ^ ((~BCa) & BCe);

      Abi ^= Di;
      BCa = ROL (Abi, 62);
      Ago ^= Do;
      BCe = ROL (Ago, 55);
      Aku ^= Du;
      BCi = ROL (Aku, 39);
      Ama ^= Da;
      BCo = ROL (Ama, 41);
      Ase ^= De;
      BCu = ROL (Ase, 2);
      Esa = BCa ^ ((~BCe) & BCi);
      Ese = BCe ^ ((~BCi) & BCo);
      Esi = BCi ^ ((~BCo) & BCu);
      Eso = BCo ^ ((~BCu) & BCa);
      Esu = BCu ^ ((~BCa) & BCe);

      //    prepareTheta
      BCa = Eba ^ Ega ^ Eka ^ Ema ^ Esa;
      BCe = Ebe ^ Ege ^ Eke ^ Eme ^ Ese;
      BCi = Ebi ^ Egi ^ Eki ^ Emi ^ Esi;
      BCo = Ebo ^ Ego ^ Eko ^ Emo ^ Eso;
      BCu = Ebu ^ Egu ^ Eku ^ Emu ^ Esu;

      // thetaRhoPiChiIotaPrepareTheta(round+1, E, A)
      Da = BCu ^ ROL (BCe, 1);
      De = BCa ^ ROL (BCi, 1);
      Di = BCe ^ ROL (BCo, 1);
      Do = BCi ^ ROL (BCu, 1);
      Du = BCo ^ ROL (BCa, 1);

      Eba ^= Da;
      BCa = Eba;
      Ege ^= De;
      BCe = ROL (Ege, 44);
      Eki ^= Di;
      BCi = ROL (Eki, 43);
      Emo ^= Do;
      BCo = ROL (Emo, 21);
      Esu ^= Du;
      BCu = ROL (Esu, 14);
      Aba = BCa ^ ((~BCe) & BCi);
      Aba ^= (uint64_t)KeccakF_RoundConstants[round + 1];
      Abe = BCe ^ ((~BCi) & BCo);
      Abi = BCi ^ ((~BCo) & BCu);
      Abo = BCo ^ ((~BCu) & BCa);
      Abu = BCu ^ ((~BCa) & BCe);

      Ebo ^= Do;
      BCa = ROL (Ebo, 28);
      Egu ^= Du;
      BCe = ROL (Egu, 20);
      Eka ^= Da;
      BCi = ROL (Eka, 3);
      Eme ^= De;
      BCo = ROL (Eme, 45);
      Esi ^= Di;
      BCu = ROL (Esi, 61);
      Aga = BCa ^ ((~BCe) & BCi);
      Age = BCe ^ ((~BCi) & BCo);
      Agi = BCi ^ ((~BCo) & BCu);
      Ago = BCo ^ ((~BCu) & BCa);
      Agu = BCu ^ ((~BCa) & BCe);

      Ebe ^= De;
      BCa = ROL (Ebe, 1);
      Egi ^= Di;
      BCe = ROL (Egi, 6);
      Eko ^= Do;
      BCi = ROL (Eko, 25);
      Emu ^= Du;
      BCo = ROL (Emu, 8);
      Esa ^= Da;
      BCu = ROL (Esa, 18);
      Aka = BCa ^ ((~BCe) & BCi);
      Ake = BCe ^ ((~BCi) & BCo);
      Aki = BCi ^ ((~BCo) & BCu);
      Ako = BCo ^ ((~BCu) & BCa);
      Aku = BCu ^ ((~BCa) & BCe);

      Ebu ^= Du;
      BCa = ROL (Ebu, 27);
      Ega ^= Da;
      BCe = ROL (Ega, 36);
      Eke ^= De;
      BCi = ROL (Eke, 10);
      Emi ^= Di;
      BCo = ROL (Emi, 15);
      Eso ^= Do;
      BCu = ROL (Eso, 56);
      Ama = BCa ^ ((~BCe) & BCi);
      Ame = BCe ^ ((~BCi) & BCo);
      Ami = BCi ^ ((~BCo) & BCu);
      Amo = BCo ^ ((~BCu) & BCa);
      Amu = BCu ^ ((~BCa) & BCe);

      Ebi ^= Di;
      BCa = ROL (Ebi, 62);
      Ego ^= Do;
      BCe = ROL (Ego, 55);
      Eku ^= Du;
      BCi = ROL (Eku, 39);
      Ema ^= Da;
      BCo = ROL (Ema, 41);
      Ese ^= De;
      BCu = ROL (Ese, 2);
      Asa = BCa ^ ((~BCe) & BCi);
      Ase = BCe ^ ((~BCi) & BCo);
      Asi = BCi ^ ((~BCo) & BCu);
      Aso = BCo ^ ((~BCu) & BCa);
      Asu = BCu ^ ((~BCa) & BCe);
    }

  // copyToState(state, A)
  state[0] = Aba;
  state[1] = Abe;
  state[2] = Abi;
  state[3] = Abo;
  state[4] = Abu;
  state[5] = Aga;
  state[6] = Age;
  state[7] = Agi;
  state[8] = Ago;
  state[9] = Agu;
  state[10] = Aka;
  state[11] = Ake;
  state[12] = Aki;
  state[13] = Ako;
  state[14] = Aku;
  state[15] = Ama;
  state[16] = Ame;
  state[17] = Ami;
  state[18] = Amo;
  state[19] = Amu;
  state[20] = Asa;
  state[21] = Ase;
  state[22] = Asi;
  state[23] = Aso;
  state[24] = Asu;
}

static void
keccak_init (shake128_state_t *state)
{
  unsigned int i;

  for (i = 0; i < 25; i++)
    state->s[i] = 0;
  state->pos = 0;
  state->final = 0;
}

static unsigned int
keccak_absorb (uint64_t s[25], unsigned int r, unsigned int pos,
               const uint8_t *m, size_t mlen)
{
  unsigned int i;
  uint8_t t[8] = { 0 };

  if (pos & 7)
    {
      i = pos & 7;
      while (i < 8 && mlen > 0)
        {
          t[i++] = *m++;
          mlen--;
          pos++;
        }
      s[(pos - i) / 8] ^= load64 (t);
    }

  if (pos && mlen >= r - pos)
    {
      for (i = 0; i < (r - pos) / 8; i++)
        s[pos / 8 + i] ^= load64 (m + 8 * i);
      m += r - pos;
      mlen -= r - pos;
      pos = 0;
      KeccakF1600_StatePermute (s);
    }

  while (mlen >= r)
    {
      for (i = 0; i < r / 8; i++)
        s[i] ^= load64 (m + 8 * i);
      m += r;
      mlen -= r;
      KeccakF1600_StatePermute (s);
    }

  for (i = 0; i < mlen / 8; i++)
    s[pos / 8 + i] ^= load64 (m + 8 * i);
  m += 8 * i;
  mlen -= 8 * i;
  pos += 8 * i;

  if (mlen)
    {
      for (i = 0; i < 8; i++)
        t[i] = 0;
      for (i = 0; i < mlen; i++)
        t[i] = m[i];
      s[pos / 8] ^= load64 (t);
      pos += mlen;
    }

  return pos;
}

static void
keccak_finalize (uint64_t s[25], unsigned int r, unsigned int pos, uint8_t p)
{
  unsigned int i, j;

  i = pos >> 3;
  j = pos & 7;
  s[i] ^= (uint64_t)p << 8 * j;
  s[r / 8 - 1] ^= 1ULL << 63;
}

static unsigned int
keccak_squeeze (uint8_t *out, size_t outlen, uint64_t s[25], unsigned int r,
                unsigned int pos)
{
  unsigned int i;
  uint8_t t[8];

  if (pos & 7)
    {
      store64 (t, s[pos / 8]);
      i = pos & 7;
      while (i < 8 && outlen > 0)
        {
          *out++ = t[i++];
          outlen--;
          pos++;
        }
    }

  if (pos && outlen >= r - pos)
    {
      for (i = 0; i < (r - pos) / 8; i++)
        store64 (out + 8 * i, s[pos / 8 + i]);
      out += r - pos;
      outlen -= r - pos;
      pos = 0;
    }

  while (outlen >= r)
    {
      KeccakF1600_StatePermute (s);
      for (i = 0; i < r / 8; i++)
        store64 (out + 8 * i, s[i]);
      out += r;
      outlen -= r;
    }

  if (!outlen)
    return pos;
  else if (!pos)
    KeccakF1600_StatePermute (s);

  for (i = 0; i < outlen / 8; i++)
    store64 (out + 8 * i, s[pos / 8 + i]);
  out += 8 * i;
  outlen -= 8 * i;
  pos += 8 * i;

  store64 (t, s[pos / 8]);
  for (i = 0; i < outlen; i++)
    out[i] = t[i];
  pos += outlen;
  return pos;
}

