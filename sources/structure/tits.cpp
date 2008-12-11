/*!
\file
\brief Implementation of the classes TitsGroup and TitsElt.

  This module contains an implementation of a slight variant of the
  Tits group (also called extended Weyl group) as defined in J. Tits,
  J. of Algebra 4 (1966), pp. 96-116.

  The slight variant is that we include all elements of order two in the
  torus, instead of just the subgroup generated by the \f$m_\alpha\f$ (denoted
  \f$h_\alpha\f$ in Tits' paper.) Tits' original group may be defined by
  generators \f$\sigma_\alpha\f$ for \f$\alpha\f$ simple, subject to the braid
  relations and to \f$\sigma_\alpha^2= m_\alpha\f$; to get our group we just
  add a basis of elements of $H(2)$ as additional generators, and express the
  \f$m_\alpha\f$ in this basis. This makes for a simpler implementation, where
  totus parts are just elements of the $\Z/2\Z$-vector space $H(2)$.

  On a practical level, because the \f$\sigma_\alpha\f$ satisfy the braid
  relations, any element of the Weyl group has a canonical lifting in the Tits
  group; so we may uniquely represent any element of the Tits group as a pair
  $(t,w)$, with $t$ in $T(2)$ and $w$ in $W$ (the latter representing the
  canonical lift \f$\sigma_w\f$. The multiplication rules have to be
  thoroughly changed, though, to take into account the new relations.

  We have not tried to be optimally efficient here, as it is not
  expected that Tits computations will be significant computationally.
*/
/*
  This is tits.cpp

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Reductive Lie Groups

  For license information see the LICENSE file
*/

#include "tits.h"

#include "lattice.h"
#include "rootdata.h"
#include "setutils.h"

namespace atlas {


/****************************************************************************

        Chapter II -- The TitsGroup class

*****************************************************************************/

namespace tits {

/*!
  \brief Constructs the Tits group corresponding to the root datum |rd|, and
  the fundamental involution |d|.
*/
TitsGroup::TitsGroup(const rootdata::RootDatum& rd,
		     const latticetypes::LatticeMatrix& d,
		     const weyl::Twist& twist)
  : d_weyl(*new weyl::TwistedWeylGroup(rd.cartanMatrix(),twist))
  , d_rank(rd.rank())
  , d_simpleRoot(rd.semisimpleRank())   // set number of vectors, but not yet
  , d_simpleCoroot(rd.semisimpleRank()) // their size (which will be |d_rank|)
  , d_involution(d.transposed())
{
  for (size_t j = 0; j < rd.semisimpleRank(); ++j) // reduce vectors mod 2
  {
    d_simpleRoot[j]  =latticetypes::SmallBitVector(rd.simpleRoot(j));
    d_simpleCoroot[j]=latticetypes::SmallBitVector(rd.simpleCoroot(j));
  }
}

// Switching between left and right torus parts is a fundamental tool.

/*!
  \brief find torus part $x'$ so that $x.w=w.x'$

  Algorithm: for simple reflections this is |reflect|; repeat left-to-right

  Note: a more sophisticated algorithm would involve precomputing the
  conjugation matrices for the various pieces of w. Don't do it unless it
  turns out to really matter.
*/
TorusPart TitsGroup::push_across(TorusPart x, const weyl::WeylElt& w) const
{
  weyl::WeylWord ww=d_weyl.word(w);

  for (size_t j = 0; j < ww.size(); ++j)
    reflect(x,ww[j]);

  return x;
}

// find torus part $x'$ so that $w.x=x'.w$; inverse to |push_across|
TorusPart TitsGroup::pull_across(const weyl::WeylElt& w, TorusPart x) const
{
  weyl::WeylWord ww=d_weyl.word(w);
  for (size_t j=ww.size(); j-->0; )
    reflect(x,ww[j]);
  return x;
}


/*!
  \brief Left multiplication of |a| by the canonical generator \f$\sigma_s\f$.

  This is the basic case defining the group structure in the Tits group (since
  left-multiplication by an element of $T(2)$ just adds to the torus part).

  Algorithm: This is surprisingly simple: multiplying by \f$\sigma_s\f$ just
  amounts to reflecting the torus part through |s|, then left-multiplying the
  Weyl group part $w$ by |s| in the Weyl group, and if the length goes down in
  the latter step, add a factor of \f$(\sigma_s)^2=m_s\f$ to the torus part.
  (To see this, write in the length-decreasing case $w=s.w'$ with $w'$
  reduced; then \f$\sigma_s\sigma_w=m_s\sigma_{w'}\f$, so we need to add $m_s$
  to the reflected left torus part in that case.

  The upshot is a multiplication algorithm almost as fast as in the Weyl group!
*/
void TitsGroup::sigma_mult(weyl::Generator s,TitsElt& a) const
{
  reflect(a.d_t,s); // commute (left) torus part with $\sigma_s$
  if (d_weyl.leftMult(a.d_w,s)<0) // torus part adjusted on length decrease
    left_add(d_simpleCoroot[s],a);
}

void TitsGroup::sigma_inv_mult(weyl::Generator s,TitsElt& a) const
{
  reflect(a.d_t,s); // commute (left) torus part with $\sigma_s$
  if (d_weyl.leftMult(a.d_w,s)>0) // torus part adjusted on length increase
    left_add(d_simpleCoroot[s],a);
}


/*!
  \brief Right multiplication of |a| by the canonical generator \f$sigma_s\f$.

  Algorithm: similar to above, but omitting the |reflect| (since the torus
  part is at the left), and using |weyl::mult| instead of |weyl::leftMult|,
  and |right_add| to add the possible contribution $m_s$ instead of
  |left_add|; the latter implicitly involves a call to |pull_across|.
*/
void TitsGroup::mult_sigma(TitsElt& a, weyl::Generator s) const
{
// |WeylGroup::mult| multiplies $w$ by $s$, returns sign of the length change
  if (d_weyl.mult(a.d_w,s)<0) // torus part needs adjustment on length decrease
    right_add(a,d_simpleCoroot[s]);
}

void TitsGroup::mult_sigma_inv(TitsElt& a, weyl::Generator s) const
{
  if (d_weyl.mult(a.d_w,s)>0)// torus part needs adjustment on length increase
    right_add(a,d_simpleCoroot[s]);
}


/*!
  \brief Product of general Tits group elements

  Algorithm: The algorithm is to multiply the second factors successively by
  the generators in a reduced expression for |a.w()|, then left-add |a.t()|.

  Since we have torus parts on the left, we prefer left-multiplication here.
*/
TitsElt TitsGroup::prod(const TitsElt& a, TitsElt b) const
{
  weyl::WeylWord ww=d_weyl.word(a.w());

  // first incorporate the Weyl group part
  for (size_t j = ww.size(); j-->0; )
    sigma_mult(ww[j],b);

  // and finally the torus part
  left_add(a.d_t,b);
  return b;
}


} // namespace tits


} // namespace atlas
