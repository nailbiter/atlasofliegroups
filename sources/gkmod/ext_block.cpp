/*
  This is ext_block.cpp

  Copyright (C) 2013-2014 Marc van Leeuwen
  Part of the Atlas of Lie Groups and Representations

  For license information see the LICENSE file
*/

#include "ext_block.h"

#include <cassert>
#include <vector>

#include "blocks.h"
#include "weyl.h"

#include "bitmap.h"
#include "polynomials.h"
/*
  For an extended group, the block structure is more complicated than an
  ordinary block, because each link in fact represents a local part of the
  parent block structure that links a twist-fixed block element to another
  twist-fixed element, via intermediate elements that are non twist-fixed.
 */

namespace atlas {

namespace ext_block {

bool is_complex(DescValue v)
{
  static unsigned long mask =
    1ul << one_complex_ascent   | 1ul << one_complex_descent |
    1ul << two_complex_ascent   | 1ul << two_complex_descent |
    1ul << three_complex_ascent | 1ul << three_complex_descent;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool has_double_image(DescValue v)
{
  static unsigned long mask =
       1ul << one_real_pair_fixed         | 1ul << one_imaginary_pair_fixed
    |  1ul << two_real_double_double      | 1ul << two_imaginary_double_double
    |  1ul << two_imaginary_single_double | 1ul << two_real_single_double;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool is_unique_image (DescValue v)
{
  static unsigned long mask =
      1ul << one_real_pair_fixed    | 1ul << one_imaginary_pair_fixed
    | 1ul << two_semi_imaginary     | 1ul << two_semi_real
    | 1ul << two_real_double_double | 1ul << two_imaginary_double_double
    | 1ul << three_semi_imaginary   | 1ul << three_real_semi
    | 1ul << three_imaginary_semi   | 1ul << three_semi_real;

  return (1ul << v & mask) != 0 // whether |v| is one of the above
    or is_complex(v);  // these are also unique images
}

bool is_like_nonparity(DescValue v)
{
  static unsigned long mask =
      1ul << one_imaginary_pair_switched | 1ul << one_real_nonparity
    | 1ul << two_real_nonparity          | 1ul << three_real_nonparity;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool is_like_compact(DescValue v)
{
  static unsigned long mask =
      1ul << one_real_pair_switched | 1ul << one_imaginary_compact
    | 1ul << two_imaginary_compact  | 1ul << three_imaginary_compact;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool is_like_type_1(DescValue v)
{
  static unsigned long mask =
      1ul << one_imaginary_single | 1ul << one_real_pair_fixed
    | 1ul << two_imaginary_single_single  | 1ul << two_real_double_double;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool is_like_type_2(DescValue v)
{
  static unsigned long mask =
      1ul << one_imaginary_pair_fixed | 1ul << one_real_single
    | 1ul << two_imaginary_double_double  | 1ul << two_real_single_single;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool has_quadruple(DescValue v)
{
  static const unsigned long mask =
    1ul << two_imaginary_single_double | 1ul << two_real_single_double;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

bool is_proper_ascent(DescValue v)
{
  return not(is_descent(v) or is_like_nonparity(v));
}

bool has_defect(DescValue v)
{
  static unsigned long mask =
      1ul << two_semi_imaginary   | 1ul << two_semi_real
    | 1ul << three_semi_imaginary | 1ul << three_real_semi
    | 1ul << three_imaginary_semi | 1ul << three_semi_real;

  return (1ul << v & mask) != 0; // whether |v| is one of the above
}

int generator_length(DescValue v)
{ return v<two_complex_ascent ? 1 : v<three_complex_ascent ? 2 : 3; }

// find element |n| such that |z(n)>=zz|
BlockElt extended_block::element(BlockElt zz) const
{
  BlockElt n=0;
  while (n<size() and z(n)<zz)
    ++n;
  return n;
}


bool extended_block::toggle_edge(BlockElt x,BlockElt y, bool verbose)
{
  x = element(x); y=element(y);
  assert (x!=UndefBlock and y!=UndefBlock);
  BlockEltPair p= x<y ? std::make_pair(x,y) : std::make_pair(y,x);
  std::pair<std::set<BlockEltPair>::iterator,bool>
    inserted = flipped_edges.insert(p);
  if (not inserted.second)
    flipped_edges.erase(inserted.first);

  if (verbose)
    std::cerr << (inserted.second ? "Set" : "Unset") << " edge ("
	      << z(p.first) << ',' << z(p.second) << ')' << std::endl;

  return inserted.second;
}

void extended_block::order_quad
  (BlockElt x,BlockElt y, BlockElt p, BlockElt q, int s, bool verbose)
{
  x = element(x); y=element(y); // decipher user friendly numbering
  p = element(p); q=element(q);
  assert (x!=UndefBlock and y!=UndefBlock and p!=UndefBlock and q!=UndefBlock);
  assert (descent_type(s-1,x)==two_imaginary_single_double);
  assert (descent_type(s-1,y)==two_imaginary_single_double);
  assert (descent_type(s-1,p)==two_real_single_double);
  assert (descent_type(s-1,q)==two_real_single_double);
  const BlockEltPair xy(x,y);
  const BlockEltPair pq(p,q);
  std::vector<block_fields>& data_s = data[s-1];
  data_s[x].links = data_s[y].links = pq;
  data_s[p].links = data_s[q].links = xy;
  if (verbose)
    std::cerr << "Ordering (" << z(x) << ',' << z(y) << ';'
	      << z(p) << ',' << z(q) << ") for generator " << s << std::endl;
}

BlockElt extended_block::cross(weyl::Generator s, BlockElt n) const
{
  switch (descent_type(s,n))
  {
  case one_complex_ascent:
  case one_complex_descent:
  case two_complex_ascent:
  case two_complex_descent:
  case three_complex_ascent:
  case three_complex_descent:
    return data[s][n].links.first;

    // zero valued Cayleys have trivial cross actions
  case one_real_nonparity: case one_imaginary_compact:
  case two_real_nonparity: case two_imaginary_compact:
  case three_real_nonparity: case three_imaginary_compact:

    // double valued Cayleys also have trivial cross actions
  case one_real_pair_fixed: case one_real_pair_switched:
  case one_imaginary_pair_fixed: case one_imaginary_pair_switched:
  case two_real_double_double: case two_imaginary_double_double:

    // cases with back-and-forth cross actions
  case two_semi_imaginary: case two_semi_real:
  case two_imaginary_single_double: case two_real_single_double:
  case three_semi_imaginary: case three_real_semi:
  case three_imaginary_semi: case three_semi_real:
    return n;

    // single valued extended Cayleys use second link for cross action
  case one_imaginary_single:
  case one_real_single:
  case two_imaginary_single_single:
  case two_real_single_single:
    return data[s][n].links.second;

  }
  assert(false); return UndefBlock; // keep compiler happy
} // |extended_block::cross|

BlockElt extended_block::Cayley(weyl::Generator s, BlockElt n) const
{
  const DescValue type = descent_type(s,n);
  return
    is_descent(type) or is_complex(type) ? UndefBlock : data[s][n].links.first;
}

BlockElt extended_block::inverse_Cayley(weyl::Generator s, BlockElt n) const
{
  const DescValue type = descent_type(s,n);
  return
    not is_descent(type) or is_complex(type) ? UndefBlock
    : data[s][n].links.first;
}

BlockElt extended_block::some_scent(weyl::Generator s, BlockElt n) const
{
  const BlockElt c = data[s][n].links.first;
  assert(c!=UndefBlock);
  return c;
}

void extended_block::add_neighbours
  (BlockEltList& dst, weyl::Generator s, BlockElt n) const
{
  const BlockEltPair& links = data[s][n].links;
  if (links.first==UndefBlock)
    return;
  dst.push_back(links.first);
  if (links.second==UndefBlock)
    return;
  dst.push_back(links.second);
}

BlockEltPair extended_block::Cayleys(weyl::Generator s, BlockElt n) const
{
  const DescValue type = descent_type(s,n);
  BlockEltPair result(UndefBlock,UndefBlock);
  if (not is_descent(type) and not is_complex(type))
  {
    result.first = data[s][n].links.first;
    if (has_double_image(type))
      result.second = data[s][n].links.second;
  }
  return result;
}

BlockEltPair
extended_block::inverse_Cayleys(weyl::Generator s, BlockElt n) const
{
  const DescValue type = descent_type(s,n);
  BlockEltPair result(UndefBlock,UndefBlock);
  if (is_descent(type) and not is_complex(type))
  {
    result.first = data[s][n].links.first;
    if (has_double_image(type))
      result.second = data[s][n].links.second;
  }
  return result;
}

// whether link for |s| from |x| to |y| has a signe flip attached
int extended_block::epsilon(weyl::Generator s, BlockElt x, BlockElt y ) const
{
  BlockEltPair p= x<y ? std::make_pair(x,y) : std::make_pair(y,x);
  int sign = flipped_edges.count(p)==0 ? 1 : -1;

  // each 2i12/21r21 quadruple has one negative sign not using |flipped_edges|
  if (has_quadruple(descent_type(s,x)) and
      data[s][x].links.second==y and data[s][y].links.second==x)
    sign = -sign; // it is between second elements in both pairs of the quad

  return sign;
}

BlockEltList extended_block::down_set(BlockElt n) const
{
  BlockEltList result; result.reserve(rank());
  for (weyl::Generator s=0; s<rank(); ++s)
  {
    const DescValue type = descent_type(s,n);
    if (is_descent(type) and not is_like_compact(type))
    {
      result.push_back(data[s][n].links.first);
      if (has_double_image(type))
	result.push_back(data[s][n].links.second);
    }
  }
  return result;
}

DescValue extended_type(const Block_base& block, BlockElt z, ext_gen p,
			BlockElt& link)
{
  switch (p.type)
  {
  case ext_gen::one:
    switch (block.descentValue(p.s0,z))
    {
    case DescentStatus::ComplexAscent:
      link=block.cross(p.s0,z); return one_complex_ascent;
    case DescentStatus::ComplexDescent:
      link=block.cross(p.s0,z); return one_complex_descent;
    case DescentStatus::RealNonparity:
      link=UndefBlock; return one_real_nonparity;
    case DescentStatus::ImaginaryCompact:
      link=UndefBlock; return one_imaginary_compact;
    case DescentStatus::ImaginaryTypeI:
      link=block.cayley(p.s0,z).first; return one_imaginary_single;
    case DescentStatus::RealTypeII:
      link=block.inverseCayley(p.s0,z).first; return one_real_single;
    case DescentStatus::ImaginaryTypeII:
      link=block.cayley(p.s0,z).first;
      if (link!=UndefBlock and block.Hermitian_dual(link)==link)
	return one_imaginary_pair_fixed;
      link=UndefBlock; return one_imaginary_pair_switched;
    case DescentStatus::RealTypeI:
      link=block.inverseCayley(p.s0,z).first;
      if (link!=UndefBlock and block.Hermitian_dual(link)==link)
	return one_real_pair_fixed;
      link=UndefBlock; return one_real_pair_switched;
    }
  case ext_gen::two:
    switch (block.descentValue(p.s0,z))
    {
    case DescentStatus::ComplexAscent:
      link=block.cross(p.s0,z);
      if (link==block.cross(p.s1,z))
	return two_semi_imaginary; // just a guess if |link==UndefBlock|
      if (link!=UndefBlock)
	link=block.cross(p.s1,link);
      return two_complex_ascent;
    case DescentStatus::ComplexDescent:
      link=block.cross(p.s0,z);
      if (link==block.cross(p.s1,z))
	return two_semi_real; // just a guess if |link==UndefBlock|
      if (link!=UndefBlock)
	link=block.cross(p.s1,link);
      return two_complex_descent;
    case DescentStatus::RealNonparity:
      link=UndefBlock; return two_real_nonparity;
    case DescentStatus::ImaginaryCompact:
      link=UndefBlock; return two_imaginary_compact;
    case DescentStatus::ImaginaryTypeI:
      link=block.cayley(p.s0,z).first;
      if (link==UndefBlock)
	return two_imaginary_single_single; // really just a guess
      link=block.cayley(p.s1,link).first;
      if (link==UndefBlock)
	return two_imaginary_single_single; // really just a guess
      assert(block.Hermitian_dual(link)==link);
      return block.descentValue(p.s0,link)==DescentStatus::RealTypeI
	? two_imaginary_single_single : two_imaginary_single_double;
    case DescentStatus::RealTypeII:
      link=block.inverseCayley(p.s0,z).first;
      if (link==UndefBlock)
	return two_real_single_single; // really just a guess
      link=block.inverseCayley(p.s1,link).first;
      if (link==UndefBlock)
	return two_real_single_single; // really just a guess
      assert(block.Hermitian_dual(link)==link);
      return block.descentValue(p.s0,link)==DescentStatus::ImaginaryTypeII
	? two_real_single_single : two_real_single_double;
    case DescentStatus::ImaginaryTypeII:
      link=block.cayley(p.s0,z).first;
      if (link==UndefBlock)
	return two_imaginary_double_double;
      link=block.cayley(p.s1,link).first;
      if (link==UndefBlock)
	return two_imaginary_double_double;
      if (block.Hermitian_dual(link)!=link)
      {
	link=block.cross(p.s0,link);
	assert(link==UndefBlock or block.Hermitian_dual(link)==link);
      }
      return two_imaginary_double_double;
    case DescentStatus::RealTypeI:
      link=block.inverseCayley(p.s0,z).first;
      if (link==UndefBlock)
	return two_real_double_double;
      link=block.inverseCayley(p.s1,link).first;
      if (link==UndefBlock)
	return two_real_double_double;
      if (block.Hermitian_dual(link)!=link)
      {
	link=block.cross(p.s0,link);
	assert(link==UndefBlock or block.Hermitian_dual(link)==link);
      }
      return two_real_double_double;
    }
  case ext_gen::three:
    switch (block.descentValue(p.s0,z))
    {
    case DescentStatus::RealNonparity:
      link=UndefBlock; return three_real_nonparity;
    case DescentStatus::ImaginaryCompact:
      link=UndefBlock; return three_imaginary_compact;
    case DescentStatus::ComplexAscent:
      link=block.cross(p.s0,z);
      if (link==UndefBlock)
	return three_complex_ascent; // just a guess
      if (link==block.cross(p.s1,link))
      {
	assert(block.descentValue(p.s1,link)==
	       DescentStatus::ImaginaryTypeII);
	link=block.cayley(p.s1,link).first;
	if (link!=UndefBlock and block.Hermitian_dual(link)!=link)
	{
	  link=block.cross(p.s1,link);
	  assert(link==UndefBlock or block.Hermitian_dual(link)==link);
	}
	return three_semi_imaginary;
      }
      link=block.cross(p.s1,link);
      if (link!=UndefBlock)
	link=block.cross(p.s0,link);
      if (link!=UndefBlock)
	assert(block.Hermitian_dual(link)==link);
      return three_complex_ascent;
    case DescentStatus::ComplexDescent:
      link=block.cross(p.s0,z);
      if (link==UndefBlock)
	return three_complex_descent; // just a guess
      if (link==block.cross(p.s1,link))
      {
	assert(block.descentValue(p.s1,link)==DescentStatus::RealTypeI);
	link=block.inverseCayley(p.s1,link).first;
	if (link!=UndefBlock and block.Hermitian_dual(link)!=link)
	{
	  link=block.cross(p.s1,link);
	  assert(link==UndefBlock or block.Hermitian_dual(link)==link);
	}
	return three_semi_real;
      }
      link=block.cross(p.s1,link);
      if (link!=UndefBlock)
	link=block.cross(p.s0,link);
      if (link!=UndefBlock)
	assert(block.Hermitian_dual(link)==link);
      return three_complex_descent;
    case DescentStatus::ImaginaryTypeI:
      link=block.cayley(p.s0,z).first;
      if (link!=UndefBlock)
      {
	link=block.cross(p.s1,link);
	if (block.cayley(p.s1,z).first!=UndefBlock)
	  assert(link==block.cross(p.s0,block.cayley(p.s1,z).first));
      }
      else if ((link=block.cayley(p.s1,z).first)!=UndefBlock)
	link=block.cross(p.s0,link);
      if (link!=UndefBlock)
	assert(block.Hermitian_dual(link)==link);
      return three_imaginary_semi;
    case DescentStatus::RealTypeII:
      link=block.inverseCayley(p.s0,z).first;
      if (link!=UndefBlock)
      {
	link=block.cross(p.s1,link);
	if (block.inverseCayley(p.s1,z).first!=UndefBlock)
	  assert(link==block.cross(p.s0,block.inverseCayley(p.s1,z).first));
      }
      else if ((link=block.inverseCayley(p.s1,z).first)!=UndefBlock)
	link=block.cross(p.s0,link);
      if (link!=UndefBlock)
	assert(block.Hermitian_dual(link)==link);
      return three_real_semi;
    case DescentStatus::ImaginaryTypeII: case DescentStatus::RealTypeI:
      assert(false); // these cases should never occur
    }
  } // |switch (p.type)|
  assert(false); return one_complex_ascent; // keep compiler happy
} // |extended_type|

extended_block::extended_block
  (const Block_base& block,const TwistedWeylGroup& W)
  : parent(block)
  , tW(W)
  , folded(blocks::folded(block.Dynkin(),block.fold_orbits()))
  , info()
  , data(parent.folded_rank())
  , l_start(parent.length(parent.size()-1)+2)
  , flipped_edges()
{
  unsigned int folded_rank = data.size();
  if (folded_rank==0 or parent.Hermitian_dual(0)==UndefBlock)
    return; // block not stable under twist, so leave extended block empty

  std::vector<BlockElt> child_nr(parent.size(),UndefBlock);
  std::vector<BlockElt> parent_nr;

  { // compute |child_nr| and |parent_nr| tables, and the |l_start| vector
    size_t cur_len=0; l_start[cur_len]=0;
    for (BlockElt z=0; z<parent.size(); ++z)
      if (parent.Hermitian_dual(z)==z)
      {
	child_nr[z]=parent_nr.size();
	parent_nr.push_back(z);
	while (cur_len<parent.length(z)) // for new length level(s) reached
	  l_start[++cur_len]=child_nr[z]; // mark element as first of |cur_len|
      }
    assert(cur_len+1<l_start.size());
    l_start[++cur_len]=parent.size(); // makes |l_start[length(...)+1]| legal
  }

  info.reserve(parent_nr.size());
  for (weyl::Generator s=0; s<folded_rank; ++s)
    data[s].reserve(parent_nr.size());

  for (BlockElt n=0; n<parent_nr.size(); ++n)
  {
    BlockElt z=parent_nr[n];
    info.push_back(elt_info(z));
    info.back().length = parent.length(z);
    for (weyl::Generator s=0; s<folded_rank; ++s)
    {
      BlockElt link, second = UndefBlock;
      DescValue type = extended_type(block,z,parent.orbit(s),link);
      data[s].push_back(block_fields(type)); // create entry for block element
      if (link==UndefBlock)
	continue; // |s| done for imaginary compact and real nonparity cases

      switch (type) // maybe set |second|, depending on case
      {
      default: break;

      case one_imaginary_single:
      case one_real_single: // in these cases: parent cross neighbour for |s|
	second = parent.cross(parent.orbit(s).s0,z);
	break;

      case one_real_pair_fixed:
      case one_imaginary_pair_fixed: // in these cases: second Cayley image
	second = parent.cross(parent.orbit(s).s0,link);
	break;

      case two_imaginary_single_single:
      case two_real_single_single: // here: double cross neighbour for |s|
	if (parent.cross(parent.orbit(s).s0,z)==UndefBlock)
	  if (parent.cross(parent.orbit(s).s1,z)==UndefBlock)
	    continue; // can't get there, let's hope it doesn't exist
	  else second = parent.cross(parent.orbit(s).s0,
				     parent.cross(parent.orbit(s).s1,z));
	else
	{
	  second = parent.cross(parent.orbit(s).s1,
				parent.cross(parent.orbit(s).s0,z));
	  if (parent.cross(parent.orbit(s).s1,z)!=UndefBlock)
	    assert(parent.cross(parent.orbit(s).s0,
				parent.cross(parent.orbit(s).s1,z))==second);
	}
	break;

      case two_imaginary_single_double:
      case two_real_single_double: // find second Cayley image, which is
	second = parent.cross(parent.orbit(s).s0,link); // parent cross link
	assert(parent.cross(parent.orbit(s).s1,link)==second); // (either gen)
	if (link>second) // to make sure ordering is same for a twin pair
	  std::swap(link,second); // we order both by block number (for now)
	break;

      case two_imaginary_double_double:
      case two_real_double_double: // find second Cayley image
	if (parent.cross(parent.orbit(s).s0,link)==UndefBlock)
	  if (parent.cross(parent.orbit(s).s1,link)==UndefBlock)
	    continue; // can't get there, let's hope it doesn't exist
	  else second = parent.cross(parent.orbit(s).s0,
				     parent.cross(parent.orbit(s).s1,link));
	else
	{
	  second = parent.cross(parent.orbit(s).s1,
				parent.cross(parent.orbit(s).s0,link));
	  if (parent.cross(parent.orbit(s).s1,link)!=UndefBlock)
	    assert(parent.cross(parent.orbit(s).s0,
				parent.cross(parent.orbit(s).s1,link))
		   ==second);
	}
	break;
      } // |switch(type)|

      // enter translations of |link| and |second| to child block numbering
      BlockEltPair& dest = data[s].back().links;
      dest.first=child_nr[link];
      if (second!=UndefBlock)
	dest.second = child_nr[second];
    }
  } // |for(n)|
} // |extended_block::extended_block|

// coefficient of neighbour |sx| for $s$ in action $(T_s+1)*a_x$
Pol extended_block::T_coef(weyl::Generator s, BlockElt sx, BlockElt x) const
{
  DescValue v = descent_type(s,x);
  if (not is_descent(v))
  {
    if (x==sx) // diagonal coefficient
      if (has_defect(v))
	return Pol(1,1)+Pol(1); // $q+1$
      else  // $0$, $1$, or $2$
	return Pol(is_like_nonparity(v) ? 0 : has_double_image(v) ? 2 : 1);
    else if (is_like_type_1(v) and sx==cross(s,x)) // type 1 imaginary cross
    {
      BlockElt y = Cayley(s,x); // pass via this element for signs
      int sign = epsilon(s,x,y)*epsilon(s,sx,y); // combine two Cayley signs
      return Pol(sign);
    }
    else // below diagonal coefficient
    {
      assert (data[s][x].links.first==sx or data[s][x].links.second==sx);
      int sign = epsilon(s,x,sx);
      if (has_defect(v)) // $\pm(q+1)$
	return Pol(1,sign)+Pol(sign);
      else
	return Pol(sign); // $\pm1$
    }
  } // |if (not is_descent(v))|

  int k = orbit(s).length();
  Pol result(k,1); // start with $q^k$
  if (x==sx) // diagonal coefficient
  {
    if (has_double_image(v))  // diagonal coefficient
      result[0] = -1; // $q^k-1$
    else if (is_like_compact(v))
      result[0] = 1; // $q^k+1$
    else if (has_defect(v))
      result[1] = -1; // $q^k-q$
    // |else| leave $q^k$
  }
  else if (is_like_type_2(v) and sx==cross(s,x)) // type 2 real cross
  {
    BlockElt y = inverse_Cayley(s,x); // pass via this element for signs
    int sign = epsilon(s,y,x)*epsilon(s,y,sx); // combine two Cayley signs
    return Pol(-sign); // forget term $q^k$, return $\mp 1$ instead
  }
  else // remaining cases involve descending edge (above-diagonal coefficient)
  {
    assert (data[s][x].links.first==sx or data[s][x].links.second==sx);
    if (not is_complex(v))
      result[has_defect(v) ? 1 : 0] = -1; // change into $q^k-1$ or $q^k-q$
    result *= epsilon(s,x,sx); // flip sign according to chosen edge
  }

  return result;
} // |extended_block::T_coef|

} // |namespace ext_block|

} // |namespace atlas|
