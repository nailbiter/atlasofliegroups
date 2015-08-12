/*!
\file
\brief Class definition and function declarations for KLSupport.
*/
/*
  This is klsupport.h

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Lie Groups and Representations

  For license information see the LICENSE file
*/

#ifndef KLSUPPORT_H  /* guard against multiple inclusions */
#define KLSUPPORT_H


#include "bitset.h"	// containment

#include "atlas_types.h"
#include "blocks.h"	// inlining of methods like |cross| and |cayley|

namespace atlas {

/******** function declarations *********************************************/

/******** type definitions **************************************************/

namespace klsupport {

class KLSupport
{
  enum State {PrimitivizeFilled, DownsetsFilled, LengthLessFilled, Filled,
	      NumStates};

  BitSet<NumStates> d_state;

  const Block_base& d_block;  // non-owned reference

  std::vector<RankFlags> d_descent;
  std::vector<RankFlags> d_goodAscent;
  std::vector<BitMap> d_downset;
  std::vector<BitMap> d_primset;
  std::vector<BlockElt> d_lengthLess;
  std::vector<std::vector<unsigned int> > d_prim_index;

 public:

// constructors and destructors
  KLSupport(const Block_base&);

// copy and swap (use automatically generated copy constructor)
  void swap(KLSupport&);

// accessors
  const Block_base& block() const { return d_block; }
  size_t rank() const { return d_block.rank(); }
  size_t size() const { return d_block.size(); }

  size_t length(BlockElt z) const { return d_block.length(z); }
  BlockElt lengthLess(size_t l) const // number of block elements of length < l
   { return d_lengthLess[l]; }

  BlockElt cross(size_t s, BlockElt z) const  { return d_block.cross(s,z); }
  BlockEltPair cayley(size_t s, BlockElt z) const
  { return d_block.cayley(s,z); }

  DescentStatus::Value descentValue(size_t s, BlockElt z) const
    { return d_block.descentValue(s,z); }
  const DescentStatus& descent(BlockElt y) const // combined for all |s|
    { return d_block.descent(y); }

  const RankFlags& descentSet(BlockElt z) const
    { return d_descent[z]; }
  const RankFlags& goodAscentSet(BlockElt z) const
    { return d_goodAscent[z]; }

  // find ascent for |x| that is descent for |y| if any; |longBits| if none
  unsigned int ascent_descent(BlockElt x,BlockElt y) const
  { return (descentSet(y)-descentSet(x)).firstBit(); }

  // find non-i2 ascent for |x| that is descent for |y| if any; or |longBits|
  unsigned int good_ascent_descent(BlockElt x,BlockElt y) const
  { return (goodAscentSet(x)&descentSet(y)).firstBit(); }

  /* computing KL polynomials used to spend a large amount of time evaluating
     primitivize and then to binary-search for the resulting primitve element.
     Since the primitive condition only depends on the descent set, we speed
     things up by tabulating for each descent set the map from block elements
     to the index of their primitivized counterparts.
  */
  unsigned int prim_index (BlockElt x, const RankFlags& descent_set) const
  { return d_prim_index[descent_set.to_ulong()][x]; }

  // this is where an element |y| occurs in its "own" primitive row
  unsigned int self_index (BlockElt y) const
  { return d_prim_index[descentSet(y).to_ulong()][y]; }

  // the following are filters of the bitmap
  void filter_extremal(BitMap&, const RankFlags&) const;
  void filter_primitive(BitMap&, const RankFlags&) const;

// manipulators
  void fill();
  void fillDownsets();
  void fillPrimitivize();
};

} // |namespace klsupport|

} // |namespace atlas|

#endif
