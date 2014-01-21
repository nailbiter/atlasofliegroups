/*!
\file
\brief Class definition and function declarations for class Block.
*/
/*
  This is ext_block.h

  Copyright (C) 2013 Marc van Leeuwen
  part of the Atlas of Lie Groups and Representations

  For license information see the LICENSE file
*/

#ifndef EXT_BLOCK_H  /* guard against multiple inclusions */
#define EXT_BLOCK_H

#include "atlas_types.h"

#include <cassert>
#include <iostream>
#include "blocks.h" // for the structure |ext_gen|

namespace atlas {

namespace ext_block {


// type defintions

// for correspondence of enumerations and conventional codes, see |descent_code|
enum DescValue // every even/odd pair is one of associated ascent and descent
{
  one_complex_ascent,
  one_complex_descent,
  one_imaginary_single,         // type 1, single Cayley image
  one_real_pair_fixed,          // type 1, twist-fixed inverse Cayley images
  one_imaginary_pair_fixed,     // type 2, twist-fixed Cayley images
  one_real_single,              // type 2, single inverse Cayley image
  one_imaginary_pair_switched,  // type 2, twist-switched Cayley images
  one_real_pair_switched,       // type 1, twist-switched inverse Cayley images
  one_real_nonparity,
  one_imaginary_compact,

  two_complex_ascent,           // distinct commuting complex ascents
  two_complex_descent,          // distinct commuting complex descents
  two_semi_imaginary,           // identical complex ascents
  two_semi_real,                // identical complex descents
  two_imaginary_single_single,  // commuting single-valued Cayleys
  two_real_double_double, // commuting double-valued inverse Cayleys (2-valued)
  two_imaginary_single_double,  // single-valued Cayleys become double-valued
  two_real_single_double, // single-valued inverse Cayleys become double-valued
  two_imaginary_double_double,  // commuting double-valued Cayleys (2-valued)
  two_real_single_single, // commuting single-valued inverse Cayleys
  two_real_nonparity,
  two_imaginary_compact,

  three_complex_ascent,         // distinct non-commuting complex ascents
  three_complex_descent,        // distinct non-commuting complex descents
  three_semi_imaginary,  // non-commuting complex ascents become imaginary
  three_real_semi, // non-commuting single-valued inverse Cayleys get complex
  three_imaginary_semi,  // non-commuting single-valued Cayleys become complex
  three_semi_real,       // non-commuting complex descents become real
  three_real_nonparity,
  three_imaginary_compact
}; // |enum DescValue|

class extended_block
{
  struct elt_info // per block element information
  {
    BlockElt z; // index into parent |Block_base| structure
    unsigned short length; // length for extended group
  elt_info(BlockElt zz): z(zz),length(~0) {}

  // methods that will allow building a hashtable with |info| as pool
    typedef std::vector<elt_info> Pooltype;
    size_t hashCode(size_t modulus) const { return (9*z)&(modulus-1); }
    bool operator != (const elt_info& o) const { return z!=o.z; }

  }; // |struct elt_info|

  struct block_fields // per block element and simple reflection data
  {
    DescValue type;
    BlockEltPair links; // one or two values, depending on |type|
    bool epsilon; // whether second Cayley link carries minus sign
  block_fields(DescValue t)
  : type(t),links(UndefBlock,UndefBlock),epsilon(false) {}
  };

  const Block_base& parent;
  const TwistedWeylGroup& tW;
  std::vector<elt_info> info; // its size defines the size of the block
  std::vector<std::vector<block_fields> > data;  // size |d_rank| * |size()|
  BlockEltList l_start; // where elements of given length start

 public:

// constructors and destructors
  extended_block(const Block_base& block,const TwistedWeylGroup& W);

// accessors

  size_t rank() const { return parent.folded_rank(); }
  size_t size() const { return info.size(); }

  ext_gen orbit(weyl::Generator s) const { return parent.orbit(s); }

  BlockElt z(BlockElt n) const { assert(n<size()); return info[n].z; }

  // Look up element by |x|, |y| coordinates
  BlockElt element(BlockElt z) const; // partial inverse of method |z|

  size_t length(BlockElt n) const { return info[n].length; }
  size_t l(BlockElt y, BlockElt x) const { return length(y)-length(x); }
  BlockElt length_first(size_t l) const { return l_start[l]; }

  BlockElt cross(weyl::Generator s, BlockElt n) const;
  BlockElt Cayley(weyl::Generator s, BlockElt n) const; // just one or none
  BlockElt inverse_Cayley(weyl::Generator s, BlockElt n) const; // one or none

  BlockEltPair Cayleys(weyl::Generator s, BlockElt n) const;
  BlockEltPair inverse_Cayleys(weyl::Generator s, BlockElt n) const;

  // whether $i$-th (inverse) Cayley link at |s| from |n| flips sign
  int epsilon(weyl::Generator s, BlockElt n,unsigned i) const;

  const DescValue descent_type(weyl::Generator s, BlockElt n) const
    { assert(n<size()); assert(s<rank()); return data[s][n].type; }

  // print whole block to stream (name chosen to avoid masking by |print|)
  std::ostream& print_to(std::ostream& strm) const; // defined in |block_io|


}; // |class extended_block|



// function declarations

const char* descent_code(DescValue v); // defined in |block_io|
inline bool is_descent(DescValue v) { return v%2!=0; }
bool is_complex(DescValue v);
bool is_unique_image(DescValue v);
bool has_double_image(DescValue v);
bool is_like_nonparity(DescValue v);
bool is_proper_ascent(DescValue v);
bool has_defect(DescValue v);

DescValue extended_type(const Block_base& block, BlockElt z, ext_gen p,
			BlockElt& first_link);

} // |namespace ext_block|

} // |namespace atlas|
#endif
