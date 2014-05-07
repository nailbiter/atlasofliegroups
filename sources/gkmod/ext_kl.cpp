/*
  This is ext_kl.cpp

  Copyright 2013, Marc van Leeuwen
  part of the Atlas of Lie Groups and Representations

  For license information see the LICENSE file
*/

#include "ext_kl.h"
#include "basic_io.h"

namespace atlas {
namespace ext_kl {

class PolEntry : public Pol
{
public:
  // constructors
  PolEntry() : Pol() {} // default constructor builds zero polynomial
  PolEntry(const Pol& p) : Pol(p) {} // lift polynomial to this class

  // members required for an Entry parameter to the HashTable template
  typedef std::vector<Pol> Pooltype;  // associated storage type
  size_t hashCode(size_t modulus) const; // hash function

  // compare polynomial with one from storage
  bool operator!=(Pooltype::const_reference e) const;
}; // |class KLPolEntry|

inline size_t PolEntry::hashCode(size_t modulus) const
{ const Pol& P=*this;
  if (P.isZero()) return 0;
  polynomials::Degree i=P.degree();
  size_t h=P[i]; // start with leading coefficient
  while (i-->0) h= ((h<<21)+(h<<13)+(h<<8)+(h<<5)+h+P[i]) & (modulus-1);
  return h;
}

bool PolEntry::operator!=(PolEntry::Pooltype::const_reference e) const
{
  if (degree()!=e.degree()) return true;
  if (isZero()) return false; // since degrees match
  for (polynomials::Degree i=0; i<=degree(); ++i)
    if ((*this)[i]!=e[i]) return true;
  return false; // no difference found
}


descent_table::descent_table(const ext_block::extended_block& eb)
  : descents(eb.size()), good_ascents(eb.size())
  , prim_index(1<<eb.rank(),std::vector<unsigned int>(eb.size(),0))
  , prim_flip(eb.size(),BitMap(prim_index.size()))
  , block(eb)
{
  // counts of primitive block elements, one for each descent set
  std::vector<BlockElt> prim_count(1<<eb.rank(),0);

  // following loop must decrease for primitivization calculation below
  for (BlockElt x = block.size(); x-->0; )
  {
    RankFlags desc, good_asc;
    for (weyl::Generator s=0; s<block.rank(); ++s)
    {
      ext_block::DescValue v = block.descent_type(s,x);
      if (is_descent(v))
	desc.set(s);
      else if (not has_double_image(v))
	good_asc.set(s); // good ascent: at most one upward neighbour
    }
    descents[x]=desc;
    good_ascents[x]=good_asc;

    BitMap& flip = prim_flip[x]; // place to record primitivation flips for $x$

    // compute primitivizations of |x|, storing index among primitives for |D|
    // loop must be downwards so initially index is w.r.t. descending order
    for (unsigned long desc=prim_index.size(); desc-->0;)
    {
      RankFlags D(desc); // descent set for which to primitive
      D &= good_asc;
      if (D.none()) // then element |x| is primitive for the descent set
	prim_index[desc][x] = prim_count[desc]++; // self-ref; increment count
      else
      {
	weyl::Generator s = D.firstBit();
	if (is_like_nonparity(block.descent_type(s,x)))
	  prim_index[desc][x] = ~0; // stop primitivisation with zero result
	else
	{
	  BlockElt sx = block.some_scent(s,x);
	  assert(sx>x); // ascents go up in block
	  prim_index[desc][x] = prim_index[desc][sx];
	  if ((block.epsilon(s,x,sx)<0)!=prim_flip[sx].isMember(desc))
	    flip.insert(desc);
	}
      }
    } // |for (desc)|
  } // |for(x)|

  // primitive lists will actually be stored increasing, so reverse indices
  for (unsigned long desc=prim_index.size(); desc-->0;)
  {
    BlockElt last=prim_count[desc]-1;
    for (BlockElt x = block.size(); x-->0; )
    {
      unsigned int& slot = prim_index[desc][x];
      if (slot != ~0u) // leave "cop out" indices as such
	slot = last-slot; // reverse all other indices
    }
  }

} // |descent_table| constructor

// number of primimitive elements for descents(y) of length less than y
unsigned int descent_table::col_size(BlockElt y) const
{
  BlockElt x=length_floor(y);
  if (prim_back_up(x,y)) // find last primitive $x$ of length less than $y$
    return x_index(x,y)+1; //
  return 0; // no primitives below length of |y| at all
} // |descent_table::col_size|

bool descent_table::prim_back_up(BlockElt& x, BlockElt y) const
{
  RankFlags desc=descents[y];
  while (x-->0)
    if ((good_ascents[x]&desc).none())
      return true;
  return false;
} // |descent_table::prim_back_up|

bool descent_table::extr_back_up(BlockElt& x, BlockElt y) const
{
  RankFlags desc=descents[y];
  while (x-->0)
    if (descents[x].contains(desc))
      return true; // stop when no descents of |y| are (any) ascents of |x|
  return false;
} // |descent_table::extr_back_up|

KL_table::KL_table(const ext_block::extended_block& b, std::vector<Pol>& pool)
  : aux(b), storage_pool(pool), column()
  , untwisted(b.untwisted())
{ untwisted.fill(false); }

Pol KL_table::P(BlockElt x, BlockElt y) const
{
  const kl::KLRow& col_y = column[y];
  unsigned inx=aux.x_index(x,y);
  if (inx>=col_y.size())
    return Pol(inx==aux.self_index(y) ? aux.flips(x,y) ? -1 : 1 : 0);
  return aux.flips(x,y) ? -storage_pool[col_y[inx]] : storage_pool[col_y[inx]];
}

int KL_table::mu(int i,BlockElt x, BlockElt y) const
{
  if (aux.block.length(x)+i>aux.block.length(y))
    return 0;
  unsigned d=aux.block.l(y,x)-i;
  if (d%2!=0)
    return 0;
  d/=2;
  PolRef Pxy=P(x,y);
  return Pxy.isZero() or Pxy.degree()<d ? 0 : Pxy[d];
}

Pol qk_plus_1(int k)
{
  assert(k>=1 and k<=3);
  Pol res = Pol(k,1);
  res[0] = 1;
  return res;
}

Pol qk_minus_1(int k)
{
  assert(k>=1 and k<=3);
  Pol res = Pol(k,1);
  res[0] = -1;
  return res;
}

Pol qk_minus_q(int k)
{
  assert(k>1 and k<=3);
  Pol res = Pol(k,1);
  res[1] = -1;
  return res;
}


// component of element $a_x$ in product $(T_s+1)a_{sy}$
Pol KL_table::product_comp (BlockElt x, weyl::Generator s, BlockElt sy) const
{
  const ext_block::extended_block& b = aux.block;
  switch(type(s,x))
  { default:
      assert(false); // list will only contain descent types
      return Pol();
      // complex
  case ext_block::one_complex_descent:
  case ext_block::two_complex_descent:
  case ext_block::three_complex_descent: // contribute $P_{sx,sy}+q^kP_{x,sy}$
    {
      const BlockElt sx = b.cross(s,x);
      return b.T_coef(s,x,sx)*P(sx,sy) + b.T_coef(s,x,x)*P(x,sy);
    }
    // imaginary compact, real switched
  case ext_block::one_imaginary_compact:
  case ext_block::two_imaginary_compact:
  case ext_block::three_imaginary_compact:
  case ext_block::one_real_pair_switched:
    // contribute $(q^k+1)P_{x,sy}$
    return b.T_coef(s,x,x) * P(x,sy);
    // real type 1
  case ext_block::one_real_pair_fixed:
  case ext_block::two_real_double_double:
    { // contribute $P_{x',sy}+P_{x'',sy}+(q^k-1)P_{x,sy}$
      BlockEltPair sx = b.inverse_Cayleys(s,x);
      return b.T_coef(s,x,sx.first)*P(sx.first,sy)
	+ b.T_coef(s,x,sx.second)*P(sx.second,sy)
	+ b.T_coef(s,x,x) * P(x,sy);
    }
    // real type 2
  case ext_block::one_real_single:
  case ext_block::two_real_single_single:
    { // contribute $P_{x_s,sy}+q^kP_{x,sy}-P_{s*x,sy}$
      const BlockElt s_x = b.inverse_Cayley(s,x);
      const BlockElt x_cross = b.cross(s,x);
      return b.T_coef(s,x,s_x)*P(s_x,sy)
	+ b.T_coef(s,x,x_cross)*P(x_cross,sy)
	+ b.T_coef(s,x,x) * P(x,sy);
    }
    // defect type descents: 2Cr, 3Cr, 3r
  case ext_block::two_semi_real:
  case ext_block::three_semi_real:
  case ext_block::three_real_semi:
    { // contribute $(q+1)P_{x_s,sy}+(q^k-q)P_{x,sy}$
      const BlockElt sx = b.inverse_Cayley(s,x);
      return b.T_coef(s,x,sx) * P(sx,sy)
	+ b.T_coef(s,x,x) * P(x,sy);
    }
    // epsilon case: 2r21
  case ext_block::two_real_single_double:
    { // contribute $P_{x',sy}\pm P_{x'',sy}+(q^2-1)P_{x,sy}$
      BlockEltPair sx = b.inverse_Cayleys(s,x);
      return b.T_coef(s,x,sx.first)*P(sx.first,sy)
	+ b.T_coef(s,x,sx.second)*P(sx.second,sy)
	+ b.T_coef(s,x,x) * P(x,sy);
      // return P(sx.first,sy)*b.epsilon(s,x,sx.first)
      // 	+ P(sx.second,sy)*b.epsilon(s,x,sx.second)
      // 	+ b.T_coef(s,x,x) * P(x,sy);
    }
  } // |switch(type(s,x))|
}

// shift symmetric Laurent polynomial $aq^{-1}+b+aq$ to ordinary polynomial
inline Pol m(int a,int b) { return a==0 ? Pol(b) : qk_plus_1(2)*a + Pol(1,b); }

/*
  Find $m_s(x,y)$ (symmetric Laurent polynomials in $r$) by recursive formula
  $m(x)\cong r^k p_{x,y} + def(s,x) r p_{s_x,y}-\sum_{x<u<y}p_{x,u}m(u)$ where
  $m(x)$ is $m_s(x,y)$, and congruence is modulo $r^{-1+def(s,y)}\Z[r^{-1}]$;
  then use symmetry of $m(x)$ to complete. Actual result returned is shifted
  minimally to an ordinary polynomial in $q$. There is a complication when
  $def(s,y)=1$, since a congruence modulo $\Z[r^{-1}]$ cannot be used to
  determine the coefficient of $r^0$ in $m(x)$, and instead one must use that
  the difference between the members of above congruence should be a multiple
  of $(r+r^{-1})$. To that end we use the appropriate |up_remainder(1,d)|
  values of polynomials, instead of the coefficient in $r^0$, for our
  computations. We use that |up_remainder| (with appropriate shifts) is a ring
  morphism, so can be applied to the factors in a product.
 */
Pol KL_table::get_M(weyl::Generator s, BlockElt x, BlockElt y,
		    const std::vector<Pol>& M) const
{
  const ext_block::extended_block& bl=aux.block;
  const BlockElt z =  bl.some_scent(s,y); // unique ascent by |s| of |y|

  const unsigned defect = has_defect(type(s,z)) ? 1 : 0;
  const unsigned k = bl.orbit(s).length();

  if (k==1)
    return  Pol(bl.l(y,x)%2==0 ? 0 : mu(1,x,y));

  if (k==2)
  {
    if (bl.l(y,x)%2!=0)
      return q_plus_1() * Pol(mu(1,x,y));
    if (defect==0)
    {
      int acc = mu(2,x,y);
      if (has_defect(type(s,x)))
      {
	BlockElt sx=bl.inverse_Cayley(s,x);
	acc += mu(1,sx,y)*bl.epsilon(s,sx,x); // sign is |T_coef(s,x,sx)[1]|
      }
      for (unsigned l=bl.length(x)+1; l<bl.length(z); l+=2)
	for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	  if (aux.descent_set(u)[s] and not M[u].isZero())
	    acc -= mu(1,x,u)*M[u][0];
      return Pol(acc);
    }
    // |k==2| defect case
    int acc= product_comp(x,s,y).up_remainder(1,(bl.l(z,x)+1)/2);
    for (unsigned l=bl.length(x)+2; l<bl.length(z); l+=2)
      for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	if (aux.descent_set(u)[s] and not M[u].isZero())
	  acc -= P(x,u).up_remainder(1,bl.l(u,x)/2)*M[u][0];
    return Pol(acc);
  }
  if (k==3)
  {
    if (bl.l(y,x)%2==0) // now we need a multiple of $1+q$
    {
      int acc = mu(2,x,y); // degree $1$ coefficient of |product_comp(x,s,y)|
      for (unsigned l=bl.length(x)+1; l<bl.length(z); l+=2)
	for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	  if (aux.descent_set(u)[s] and M[u].degree()==2)
	    acc -= mu(1,x,u)*M[u][2];
      return q_plus_1() * acc;
    }

    // now we need a polynomial of the form $a+bq+aq^2$ for some $a,b$
    int a = mu(1,x,y); // degree $2$ coefficient of |product_comp(x,s,y)|
    if (defect==0)
    {
      int b = mu(3,x,y);
      if (has_defect(type(s,x)))
      {
	BlockElt sx=bl.inverse_Cayley(s,x);
	b += mu(1,sx,y)*bl.epsilon(s,sx,x); // sign is |T_coef(s,x,sx)[1]|
      }
      for (BlockElt u=bl.length_first(bl.length(x)+1);
	   u<aux.length_floor(z); u++ )
	if (aux.descent_set(u)[s] and M[u].degree()==2-bl.l(u,x)%2)
	  b -= mu(M[u].degree(),x,u)*M[u][M[u].degree()];
      return m(a,b);
    }
    // remains the |k==3|, even degree $m$ defect case
    Pol Q = product_comp(x,s,y);
    if (a!=0)
      Q -= Pol((bl.l(z,x)-1)/2,qk_plus_1(2)*a); // shaves top term
    assert(2*Q.degree()<=bl.l(z,x)+1);
    int b= Q.up_remainder(1,(bl.l(z,x)+1)/2); // remainder by $q+1$
    for (unsigned l=bl.length(x)+1; l<bl.length(z); l+=2)
      for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	if (aux.descent_set(u)[s] and not M[u].isZero())
	{
	  int mu_rem = M[u].degree()==0 ? M[u][0] : M[u][1]-2*M[u][0];
	  b -= P(x,u).up_remainder(1,bl.l(u,x))*mu_rem;
	}
    return m(a,b);
  }

  // this was the general case, now unused; it always works but not optimally
  Pol Q= product_comp(x,s,y);

  for (BlockElt u=bl.length_first(bl.length(x)+1); u<aux.length_floor(z); u++)
    if (aux.descent_set(u)[s] and not M[u].isZero())
    { // subtract $q^{(d-deg(M))/2}M_u*P_{x,u}$ from contribution for $x$
      unsigned d=bl.l(z,u)+defect; // doubled implicit degree shift
      assert(M[u].degree()<=d);
      Q -= Pol((d-M[u].degree())/2,P(x,u)*M[u]);
    }

  return extract_M(Q,bl.l(z,x)+defect,defect);
} // |KL_table::get_M|

/*
  This is largely the same formula, but used in different context, which
  obliges to possibly leave out some term. Here one knows that $y$ is real
  nonparity for $s$, so in particular has no defect and the is no element $z$;
  also $x$ is known to be a descent for $s$ (unlike in the code above). On the
  other hand one is still busy computing the Hecke element $C_y$. It is a
  precondition that its coefficient $P_{x,y}$ has already been determined, and
  stored, but not necessarily $P_{x',y}$ for $x'<x$; we must therefore refrain
  from (implicit) references to such polynomials. The vector $M$ can be used
  to safely access the (complete) values $M_s(u,y)$ for all $u>x$.

  Comparing with the formulas above, the terms to skip are those involving
  |inverse_Cayley(s,x)|.
 */
Pol KL_table::get_Mp(weyl::Generator s, BlockElt x, BlockElt y,
		     const std::vector<Pol>& M) const
{
  const ext_block::extended_block& bl=aux.block;
  const unsigned k = bl.orbit(s).length();
  if (k==1) // nothing changed for this case
    return  Pol(bl.l(y,x)%2==0 ? 0 : mu(1,x,y));
  if (k==2)
  {
    if (bl.l(y,x)%2!=0)
      return q_plus_1() * Pol(mu(1,x,y));
    int acc = mu(2,x,y);
    for (unsigned l=bl.length(x)+1; l<bl.length(y); l+=2)
      for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	if (aux.descent_set(u)[s] and not M[u].isZero())
	{
	  assert(M[u].degree()==1);
	  acc -= mu(1,x,u)*M[u][1];
	}
      return Pol(acc);
  }

  assert(k==3); // this case remains
  if (bl.l(y,x)%2==0) // now we need a multiple of $1+q$
  {
    int acc = mu(2,x,y); // degree $1$ coefficient of |product_comp(x,s,y)|
    for (unsigned l=bl.length(x)+1; l<bl.length(y); l+=2)
      for (BlockElt u=bl.length_first(l); u<bl.length_first(l+1); ++u)
	if (aux.descent_set(u)[s] and M[u].degree()==2)
	  acc -= mu(1,x,u)*M[u][2];
    return q_plus_1() * acc;
  }

  // now we need a polynomial of the form $a+bq+aq^2$ for some $a,b$
  int a = mu(1,x,y); // degree $2$ coefficient of |product_comp(x,s,y)|
  int b = mu(3,x,y); // degree $0$ coefficient of |product_comp(x,s,y)|
  for (BlockElt u=bl.length_first(bl.length(x)+1); u<aux.length_floor(y); u++)
    if (aux.descent_set(u)[s] and M[u].degree()==2-bl.l(u,x)%2)
      b -= mu(M[u].degree(),x,u)*M[u][M[u].degree()];
  return m(a,b);
} // |KL_table::get_Mp|



bool KL_table::direct_recursion(BlockElt y,
				weyl::Generator& s,
				BlockElt& sy) const
{
  ext_block::DescValue v; // make value survive loop
  for (s=0; s<rank(); ++s)
  {
    v=type(s,y);
    if (is_descent(v) and is_unique_image(v))
    {
      sy = aux.block.some_scent(s,y); // some descent by $s$ of $y$
      return true;
    }
  }
  return false; // none of the generators gives a direct recursion
}


void KL_table::fill_columns(BlockElt y)
{
  PolHash hash(storage_pool); // (re)construct hash table for the polynomials
  if (y==0 or y>aux.block.size())
    y=aux.block.size(); // fill whole block if no explicit stop was indicated
  column.reserve(y);
  while (column.size()<y)
    fill_next_column(hash);
}

/* Clear terms of degree${}\geq d/2$ in $Q$ by subtracting $r^d*m$ where $m$
   is a symmetric Laurent polynomial in $r=\sqrt q$, and if $defect>0$ dividing
   what remains by $q+1$, which division must be exact. Return $r^{deg(m)}m$.
   Implemented only under the hypothesis that $Q.degree()<(d+3)/2$ initially,
   and $Q.degree()\leq d$ (so if $d=0$ then $Q$ must be constant).
 */
Pol KL_table::extract_M(Pol& Q,unsigned d,unsigned defect) const
{
  assert(2*Q.degree()<d+3 and Q.degree()<=d);
  unsigned M_deg = 2*Q.degree()-d; // might be negative; if so, unused
  Pol M(0); // result

  if (defect==0) // easy case; just pick up too high degree terms from $Q$
  {
    if (2*Q.degree()<d)
      return M; // no correction needed

    // compute $m_s(u,sy)$, the correction coefficient for $c_u$
    assert(M_deg<3);
    M=Pol(M_deg,Q[Q.degree()]);
    assert(M.degree()==M_deg); // in particular |M| is nonzero
    if (M_deg>0)
    {
      M[0] = M[M_deg]; // symmetrise if non-constant
      if (M_deg==2)
	M[1]=Q[Q.degree()-1]; // set sub-dominant coefficient here
    }
    assert(Q.degree()>=M_deg); // this explains the $Q.degree()<=d$ requirement
    Q -= Pol(Q.degree()-M_deg,M);
    return M;
  } // |if(defect==0)|

  // now $defect=1$; we must ensure that $q+1$ divides $Q-q^{(d-M_deg)/2}M$
  if (2*Q.degree()>d)
  {
    assert(M_deg!=0 and M_deg<3); // now |0<M_deg<3|
    M= Pol(M_deg,Q[Q.degree()]);
    M[0]=M[M_deg]; // symmetrise
    assert(Q.degree()>=M_deg);
    Q -= Pol(Q.degree()-M_deg,M); // subtract contribution
    assert(2*Q.degree()<=d); // terms of strictly positive degree are gone
  }
  int c = Q.factor_by(1,d/2); // division by $q+1$ is done here
  assert (c==0 or d%2==0); // if $d$ odd, there should be no remainder
  if (c==0)
    return M;

  // now add constant $c$ to $m$, since $cr^d$ had to be subtracted from $Q$
  if (M.isZero())
    M=Pol(c); // if there were no terms, create one of degree $0$
  else
  {
    assert(M_deg==2); // in which case "constant" term is in the middle
    M[1]=c;
  }
  return M;
} // |KL_table::extract_M|

void KL_table::fill_next_column(PolHash& hash)
{
  const BlockElt y = column.size();
  column.push_back(kl::KLRow());
  if (aux.col_size(y)==0)
    return; // there is just the non-recorded $P(y,y)=1$
  column.back().resize(aux.col_size(y));
  std::vector<Pol> cy(y,(Pol())),Ms(y,(Pol()));

  weyl::Generator s;
  BlockElt sy;
  if (direct_recursion(y,s,sy))
  {
    const unsigned defect = has_defect(type(s,y)) ? 1 : 0;
    const unsigned k = aux.block.orbit(s).length();
    const int sign = aux.block.epsilon(s,sy,y);
    assert(1<=k and k<=3);

    // fill array |cy| with initial contributions from $(T_s+1)*c_{sy}$
    for (BlockElt x=aux.length_floor(y); x-->0; )
      if (aux.descent_set(x)[s]) // compute contributions at all descent elts
	cy[x] = product_comp(x,s,sy);

    // next loop downwards, refining coefficient polys to meet degree bound
    for (BlockElt u=aux.length_floor(y); u-->0; )
      if (aux.descent_set(u)[s])
      {
	unsigned d=aux.block.l(y,u)+defect; // doubled degree shift of |cy[u]|
	assert(u<cy.size());
	if (cy[u].isZero())
	  continue;

	assert(u<Ms.size());
	Pol gM = get_M(s,u,sy,Ms);
	Ms[u]=extract_M(cy[u],d,defect);
	assert(Ms[u]==gM);

	if (Ms[u].isZero())
	  continue;

	d -= Ms[u].degree();
	assert (d%2==0);
	d/=2;
	// now update contributions to all lower descents |x|
	for (BlockElt x=aux.length_floor(u); x-->0; )
	  if (aux.descent_set(x)[s])
	  { // subtract $q^{(d-M_deg)/2}M_u*P_{x,u}$ from contribution for $x$
	    assert(x<cy.size());
	    cy[x] -= Pol(d,P(x,u)*Ms[u]);
	  }
      } // |for(u)|

    // finally copy relevant coefficients from |cy| array to |column[y]|
    kl::KLRow::reverse_iterator it = column.back().rbegin();
    for (BlockElt x=aux.length_floor(y); aux.prim_back_up(x,y); it++)
      if (aux.descent_set(x)[s]) // then we computed $P(x,y)$ above
        *it = hash.match(cy[x]*sign);
      else // |x| might not be descent for |s| if primitive but not extremal
      { // find and use a double-valued ascent for |x| that is decsent for |y|
	assert(has_double_image(type(s,x))); // since |s| non-good ascent
	BlockEltPair sx = aux.block.Cayleys(s,x);
	Pol Q
	  = P(sx.first,y)*aux.block.epsilon(s,x,sx.first)    // computed earlier
	  + P(sx.second,y)*aux.block.epsilon(s,x,sx.second); // in this loop
        *it = hash.match(Q);
      }
    assert(it==column.back().rend()); // check that we've traversed the column
  }
  else // direct recursion was not possible
    do_new_recursion(y,hash);

  assert(check_polys(y));
 } // |KL_table::fill_next_column|

void KL_table::do_new_recursion(BlockElt y,PolHash& hash)
{
  std::vector<Pol> cy(y,(Pol()));
  kl::KLRow::reverse_iterator out_it = column.back().rbegin();
  std::vector<weyl::Generator> rn_s; rn_s.reserve(rank());
  std::vector<std::vector<Pol> > M_s; M_s.reserve(rank());
  for (weyl::Generator s=0; s<rank(); ++s)
    if (is_like_nonparity(type(s,y)))
    {
      rn_s.push_back(s);
      M_s.push_back(std::vector<Pol>(y,Pol()));
    }

  BlockEltList downs = aux.block.down_set(y);
  for (BlockEltList::const_iterator it=downs.begin(); it!=downs.end(); ++it)
  {
    BlockElt u = *it;
    for (unsigned i=0; i<rn_s.size(); ++i)
    {
      const ext_block::DescValue tsu=type(rn_s[i],u);
      if (not is_descent(tsu) and has_defect(tsu)) // defect ascent: not-good
	std::cerr << "Bad element " << aux.block.z(u)
		  << "in down-set for " << aux.block.z(y) << std::endl;
    } // |for(i)|, loop over |s|
  } // |for(it)|, check downset

  for (BlockElt x=aux.length_floor(y); x-->0; )
  {
    if (aux.easy_set(x,y).any())
    {
      weyl::Generator s=aux.very_easy_set(x,y).firstBit();
      if (s<rank()) // non primitive case; equate to a previous polynomial
	cy[x] = is_like_nonparity(type(s,x)) ? Pol()
	  : P(aux.block.some_scent(s,x),y);
      else // do primitive but not extremal case
      {
	s = aux.easy_set(x,y).firstBit();
	assert(has_double_image(type(s,x))); // since |s| non-good ascent
	BlockEltPair sx = aux.block.Cayleys(s,x);
	cy[x]
	  = P(sx.first,y)*aux.block.epsilon(s,x,sx.first)    // computed earlier
	  + P(sx.second,y)*aux.block.epsilon(s,x,sx.second); // in this loop
      }
    }
    else // |x| is extremal for |y|, must do computation
    { // first seek proper |s|
      unsigned i; ext_block::DescValue tsx; weyl::Generator s;
      for (i=0; i<rn_s.size(); ++i)
	if (is_proper_ascent(tsx=type(s=rn_s[i],x)))
	{
	  if (not is_like_type_1(tsx))
	    break;
	  else if (aux.easy_set(aux.block.cross(s,x),y).any())
	    break;
	}

      if (i<rn_s.size())
      {
	const weyl::Generator s=rn_s[i];
	const unsigned k = aux.block.orbit(s).length();
	Pol& Q=cy[x];
	const BlockElt last_u=aux.block.length_first(aux.block.length(x)+1);
	for (BlockElt u=aux.length_floor(y); u-->last_u; )
	  if (is_descent(type(s,u)) and not M_s[i][u].isZero())
	    Q += Pol((aux.block.l(y,u)+k-M_s[i][u].degree())/2,
		     P(x,u)*M_s[i][u]);
	if (is_complex(tsx))
	{
	  BlockElt sx=aux.block.cross(s,x);
	  if (sx<y)
	    Q -= Pol(k,cy[sx]);
	}
	else if (has_defect(tsx))
	{
	  BlockElt sx=aux.block.Cayley(s,x);
	  if (sx<y)
	    Q -= qk_minus_q(k) * cy[sx];
	  int c = Q.factor_by(1,(aux.block.l(y,x)+1)/2);
	  if (aux.block.l(y,x)%2!=0)
	    assert(c==Q.coef(aux.block.l(y,x)/2));
	  else
	    assert(c==0);
	  ndebug_use(c);
	}
	else if (is_like_type_2(tsx))
	{
	  BlockEltPair sx=aux.block.Cayleys(s,x);
	  Pol S;
	  if (sx.first<y)
	    S = cy[sx.first];
	  if (sx.second<y)
	    S += cy[sx.second];
	  Q -= qk_minus_1(k)*S;
	  Q/=2;
	}
	else if (is_like_type_1(tsx))
	{ // this used to be called the endgame case
	  BlockElt x_prime=aux.block.Cayley(s,x);
	  if (x_prime<y)
	    Q -= qk_minus_1(k)*cy[x_prime];
	  BlockElt s_cross_x = aux.block.cross(s,x);
	  const weyl::Generator t=aux.easy_set(s_cross_x,y).firstBit();
	  BlockEltPair sx_up_t = aux.block.Cayleys(t,s_cross_x);
	  if (sx_up_t.first<y)
	    Q -= cy[sx_up_t.first];
	  if (sx_up_t.second<y)
	    Q -= cy[sx_up_t.second];
	}
	else
	{
	  assert(tsx==ext_block::two_imaginary_single_double);
	  BlockEltPair sx=aux.block.Cayleys(s,x);
	  Pol S;
	  if (sx.first<y)
	    S = cy[sx.first]*aux.block.epsilon(s,x,sx.first);
	  if (sx.second<y)
	    S += cy[sx.second]*aux.block.epsilon(s,x,sx.second);
	  Q -= qk_minus_1(2)*S;
	  Q/=2;
	}
	// now |Q| is stored in |cy[x]|
      } // end of |if(i<rn_s.size())|; no |else|, just leave |cy[x]==Pol(0)|
    } // end of easy/hard condition

    if (aux.very_easy_set(x,y).none())
      *out_it++ = hash.match(cy[x]); // store result whenever primitive

    // now if there is a defect ascent from x, update |M_s| for |mu(1,x,y)|
    if (aux.block.l(y,x)%2!=0 and cy[x].degree()==aux.block.l(y,x)/2)
      for (unsigned i=0; i<rn_s.size(); ++i)
      {
	const weyl::Generator s=rn_s[i];
	const ext_block::DescValue tsx=type(s,x);
	if (not is_descent(tsx) and has_defect(tsx))
	{
	  const BlockElt sx = aux.block.Cayley(s,x);
	  assert(sx!=UndefBlock);
	  M_s[i][sx] += Pol(cy[x].coef(aux.block.l(y,x)/2));
	}
      }
    // and update the entries |M_s[i][x]|
    for (unsigned i=0; i<rn_s.size(); ++i)
      if (is_descent(type(rn_s[i],x)))
	M_s[i][x] = get_Mp(rn_s[i],x,y,M_s[i]);
  } // |for(x)|
  assert(out_it==column[y].rend()); // check that we've traversed the column
} // |KL_table::do_new_recursion|


bool check(const Pol& P_sigma, const KLPol& P)
{
  if (P_sigma.isZero())
    return true;
  if (P.isZero() or P_sigma.degree()>P.degree())
    return false;
  for (polynomials::Degree i=0; i<=P.degree(); ++i)
  {
    KLCoeff d = P[i]+KLCoeff(P_sigma.coef(i));
    if (d%2!=0 or d>2*P[i])
      return false;
  }
  return true;
}

bool KL_table::check_polys(BlockElt y) const
{
  bool result = true;
  for (BlockElt x=y; x-->0; )
    if (not check(P(x,y),untwisted.klPol(aux.block.z(x),aux.block.z(y))))
    {
      std::cerr << "Mismatch at (" << aux.block.z(x) << ',' << aux.block.z(y)
		<< "): ";
      std::cerr << P(x,y) << " and "
		<< untwisted.klPol(aux.block.z(x),aux.block.z(y)) << std::endl;
      result=false;
    }
  return result;
}

} // |namespace kl|
} // |namespace atlas|