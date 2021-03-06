/*
  This is y_values.cpp.

  Copyright (C) 2011 Marc van Leeuwen
  part of the Atlas of Lie Groups and Representations

  For license information see the LICENSE file
*/

#include "y_values.h"

#include "tits.h" // |TorusPart|

#include "arithmetic.h"
#include "prerootdata.h"
#include "rootdata.h"

namespace atlas {

namespace y_values {
//              |TorusElement|


// For torus elements we keep numerator entries reduced modulo |2*d_denom|.
// Note this invariant is not enforced in the (private) raw constructor.

// compute $\exp((two ? 2 : 1)\pi i r)$, then reduce modulo $2X_*$
TorusElement::TorusElement(const RatWeight& r, bool two)
  : repr(r) // but possibly multiplied by 2 below
{ if (two)
    repr*=2;
  arithmetic::Denom_t d=2u*repr.denominator(); // we reduce modulo $2\Z^rank$
  Ratvec_Numer_t& num=repr.numerator();
  for (size_t i=0; i<num.size(); ++i)
    num[i] = arithmetic::remainder(num[i],d);
}

RatWeight TorusElement::log_pi(bool normalize) const
{
  if (normalize)
    return RatWeight(repr).normalize();
  return repr;
}

RatWeight TorusElement::log_2pi() const
{
  Ratvec_Numer_t numer = repr.numerator(); // copy
  arithmetic::Denom_t d = 2u*repr.denominator(); // make result mod Z, not 2Z
  return RatWeight(numer,d).normalize();
}

// evaluation giving rational number modulo 2
Rational TorusElement::evaluate_at (const SmallBitVector& alpha) const
{
  assert(alpha.size()==rank());
  arithmetic::Denom_t d = repr.denominator();
  arithmetic::Numer_t s = 0;
  for (auto it=alpha.data().begin(); it(); ++it)
    s += repr.numerator()[*it];
  return Rational(arithmetic::remainder(s,d+d),d);
}

// evaluation giving rational number modulo 2
Rational TorusElement::evaluate_at (const Coweight& alpha) const
{
  arithmetic::Denom_t d = repr.denominator();
  arithmetic::Numer_t n =
    arithmetic::remainder(alpha.dot(repr.numerator()),d+d);
  return Rational(n,d);
}

TorusElement TorusElement::operator +(const TorusElement& t) const
{
  TorusElement result(repr + t.repr,tags::UnnormalizedTag()); // raw ctor
  arithmetic::Numer_t d=2*result.repr.denominator(); // reduce modulo $2\Z^rank$
  Ratvec_Numer_t& num=result.repr.numerator();
  for (size_t i=0; i<num.size(); ++i)
    if (num[i] >= d) // correct if |result.repr| in interval $[2,4)$
      num[i] -= d;
  return result;
}

TorusElement TorusElement::operator -(const TorusElement& t) const
{
  TorusElement result(repr - t.repr,0); // raw constructor
  arithmetic::Numer_t d=2*result.repr.denominator(); // reduce modulo $2\Z^rank$
  Ratvec_Numer_t& num=result.repr.numerator();
  for (size_t i=0; i<num.size(); ++i)
    if (num[i]<0) // correct if |result.repr| in interval $(-2,0)$
      num[i] += d;
  return result;
}

TorusElement& TorusElement::operator+=(TorusPart v)
{
  arithmetic::Numer_t d = repr.denominator();
  for (size_t i=0; i<v.size(); ++i)
    if (v[i])
    {
      if (repr.numerator()[i]<d)
	repr.numerator()[i]+=d; // add 1/2 to coordinate
      else
	repr.numerator()[i]-=d; // subtract 1/2
    }
  return *this;
}

TorusElement& TorusElement::reduce()
{
  arithmetic::Denom_t d=2u*repr.denominator();
  Ratvec_Numer_t& num=repr.numerator();
  for (size_t i=0; i<num.size(); ++i)
    if (arithmetic::Denom_t(num[i])>=d) // avoid division if not necessary
      num[i] = arithmetic::remainder(num[i],d);
  return *this;
}


void TorusElement::simple_reflect(const PreRootDatum& prd, weyl::Generator s)
{ prd.simple_reflect(s,repr.numerator()); } // numerator is weight for |prd|

void TorusElement::reflect(const RootDatum& rd, RootNbr alpha)
{ rd.reflect(alpha,repr.numerator()); } // numerator is weight for |rd|

void TorusElement::act_by(const WeightInvolution& delta)
{ delta.apply_to(repr.numerator()); }

TorusElement TorusElement::simple_imaginary_cross
  (const RootDatum& dual_rd, // dual for pragmatic reasons
   RootNbr alpha) const // any simple-imaginary root
{
  TorusElement t(*this); // make a copy in all cases
  if (not negative_at(dual_rd.coroot(alpha))) // |alpha| is a noncompact root
    t += TorusPart(dual_rd.root(alpha)); // so add $m_\alpha$
  return t;
}


size_t y_entry::hashCode(size_t modulus) const
{
  unsigned long d= fingerprint.denominator()+1;
  const Ratvec_Numer_t& num=fingerprint.numerator();
  size_t h=nr; // start with involution number
  for (size_t i=0; i<num.size(); ++i)
    h=h*d+num[i];
  return h&(modulus-1);
}

bool y_entry::operator !=(const y_entry& y) const
{ return nr!=y.nr or fingerprint!=y.fingerprint; }


bool is_central(const WeightList& alpha, const TorusElement& t)
{
  RatWeight rw = t.as_Qmod2Z();
  arithmetic::Numer_t d = 2*rw.denominator();
  for (weyl::Generator s=0; s<alpha.size(); ++s)
    if (alpha[s].dot(rw.numerator())%d != 0) // see if division is exact
      return false;

  return true;
}

} // |namsepace y_values|

} // |namespace atlas|
