/*
  This is involurions.h

  Copyright (C) 2011 Marc van Leeuwen
  part of the Atlas of Reductive Lie Groups

  For license information see the LICENSE file
*/

#ifndef INVOLUTIONS_H  /* guard against multiple inclusions */
#define INVOLUTIONS_H

#include <vector>
#include <cassert>
#include <functional>

#include "atlas_types.h"

#include "hashtable.h"   // containment
#include "permutations.h"// containment root permutation in |InvolutionData|
#include "bitmap.h"      // containment root sets in |InvolutionData|

#include "weyl.h"        // containment of |WI_Entry|
#include "subquotient.h" // containment of |SmallSubspace|

/* The purpose of this module is to provide a central registry of (twisted)
   invlulutions, in the form of a hash table to encode them by numbers, and
   supplementary information in the form of a table indexed by those numbers.
   This information, which includes root classifcation and the (somewhat
   voluminous) involution matrix, is generated as soon a an involution is
   registered here; user code can add parallel arrays for specific uses.
 */

namespace atlas {

namespace involutions {


// this class gathers some information associated to a root datum involution
class InvolutionData
{
  Permutation d_rootInvolution; // permutation of all roots
  RootNbrSet d_imaginary, d_real, d_complex;
  RootNbrList d_simpleImaginary; // imaginary roots simple wrt subsystem
  RootNbrList d_simpleReal; // real roots simple wrt subsystem
 public:
  InvolutionData(const RootDatum& rd,
		 const WeightInvolution& theta);
  InvolutionData(const RootDatum& rd,
		 const WeightInvolution& theta,
		 const RootNbrSet& positive_subsystem);
  InvolutionData(const RootSystem& rs,
		 const RootNbrList& simple_images);
  static InvolutionData build(const RootSystem& rs,
			      const TwistedWeylGroup& W,
			      const TwistedInvolution& tw);
  void swap(InvolutionData&);
  // manipulators
private:
  void classify_roots(const RootSystem& rs); // workhorse for contructors
public:
  void cross_act(const Permutation& r_perm); // change (cheaply) to conjugate

  //accessors
  const Permutation& root_involution() const
    { return d_rootInvolution; }
  RootNbr root_involution(RootNbr alpha) const
    { return d_rootInvolution[alpha]; }
  const RootNbrSet& imaginary_roots() const  { return d_imaginary; }
  const RootNbrSet& real_roots() const       { return d_real; }
  const RootNbrSet& complex_roots() const    { return d_complex; }
  size_t imaginary_rank() const { return d_simpleImaginary.size(); }
  const RootNbrList& imaginary_basis() const
    { return d_simpleImaginary; }
  RootNbr imaginary_basis(size_t i) const
    { return d_simpleImaginary[i]; }
  size_t real_rank() const { return d_simpleReal.size(); }
  const RootNbrList& real_basis() const { return d_simpleReal; }
  RootNbr real_basis(size_t i) const { return d_simpleReal[i]; }

}; // |class InvolutionData|

typedef unsigned int InvolutionNbr;

class InvolutionTable
{
 public: // there is no danger to expose these constant references
  const RootDatum& rd;
  const WeightInvolution& delta;
  const TwistedWeylGroup& tW;


 private:
  weyl::TI_Entry::Pooltype pool;
  hashtable::HashTable<weyl::TI_Entry, InvolutionNbr> hash;

  struct record
  {
    WeightInvolution theta;
    InvolutionData id;
    unsigned int length;
    unsigned int W_length;
    SmallSubspace mod_space;

  record(const WeightInvolution& inv,
	 const InvolutionData& inv_d,
	 unsigned int l,
	 unsigned int Wl,
	 const SmallSubspace& ms)
  : theta(inv), id(inv_d),length(l), W_length(Wl), mod_space(ms) {}
  };

  std::vector<record> data;
  std::vector<BinaryMap> torus_simple_reflection;

 public:
  InvolutionTable // contructor; starts without any involutions
    (const RootDatum& , const WeightInvolution&,  const TwistedWeylGroup&);

  //accessors
  size_t size() const { return pool.size(); }
  bool unseen(const TwistedInvolution& tw) const
  { return hash.find(weyl::TI_Entry(tw))==hash.empty; }
  InvolutionNbr nr(const TwistedInvolution& tw) const
  { return hash.find(weyl::TI_Entry(tw)); }

  unsigned int semisimple_rank() const { return tW.rank(); }

  const weyl::TI_Entry& involution(InvolutionNbr n) const
  { assert(n<size()); return pool[n]; }

  const WeightInvolution& matrix(InvolutionNbr n) const
  { assert(n<size()); return data[n].theta; }

  unsigned int length(InvolutionNbr n) const
  { assert(n<size()); return data[n].length; }
  unsigned int Weyl_length(InvolutionNbr n) const
  { assert(n<size()); return data[n].W_length; }

  unsigned int length(const TwistedInvolution& tw) const
  { return length(nr(tw)); }
  unsigned int Weyl_length(const TwistedInvolution& tw) const
  { return Weyl_length(nr(tw)); }

  const Permutation& root_involution(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.root_involution(); }
  RootNbr root_involution(InvolutionNbr n,RootNbr alpha) const
  { assert(n<size()); return data[n].id.root_involution(alpha); }
  const RootNbrSet& imaginary_roots(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.imaginary_roots(); }
  const RootNbrSet& real_roots(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.real_roots(); }
  const RootNbrSet& complex_roots(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.complex_roots(); }
  size_t imaginary_rank(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.imaginary_rank(); }
  const RootNbrList& imaginary_basis(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.imaginary_basis(); }
  RootNbr imaginary_basis(InvolutionNbr n,weyl::Generator i) const
  { assert(n<size()); return data[n].id.imaginary_basis(i); }
  size_t real_rank(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.real_rank(); }
  const RootNbrList& real_basis(InvolutionNbr n) const
  { assert(n<size()); return data[n].id.real_basis(); }
  RootNbr real_basis(InvolutionNbr n,weyl::Generator i) const
  { assert(n<size()); return data[n].id.real_basis(i); }

  void reduce(TitsElt& a) const;

  // the following produces a light-weight function object calling |involution|
  class mapper
    : public std::unary_function<InvolutionNbr,const weyl::TI_Entry& >
  {
    const InvolutionTable& t;
  public:
  mapper(const InvolutionTable* tab) : t(*tab) {}
    const weyl::TI_Entry& operator() (InvolutionNbr n) const
    { return t.involution(n); }
  };
  mapper as_map() const { return mapper(this); }

  // manipulators
  InvolutionNbr add_involution(const TwistedInvolution& tw);
  InvolutionNbr add_cross(weyl::Generator s, InvolutionNbr n);

  void reserve(size_t s) { pool.reserve(s); }

private: // the space actually stored need not be exposed
  const SmallSubspace& mod_space(InvolutionNbr n) const
  { assert(n<size()); return data[n].mod_space; }

}; // |class InvolutionTable|

struct Cartan_orbit
{
  unsigned int Cartan_class_nbr;
  InvolutionNbr start,size;

  Cartan_orbit(InvolutionTable& i_tab,ComplexReductiveGroup& G, CartanNbr cn);

  bool contains(InvolutionNbr i) const { return i-start<size; }
  InvolutionNbr end() const { return start+size; }

}; // |struct Cartan_orbit|

class Cartan_orbits : public InvolutionTable
{
  std::vector<Cartan_orbit> orbit;
  std::vector<unsigned int> Cartan_index;

  unsigned int locate(InvolutionNbr i) const;
public:
  Cartan_orbits (const RootDatum& rd, const WeightInvolution& theta,
		 const TwistedWeylGroup& tW)
  : InvolutionTable(rd,theta,tW), orbit(), Cartan_index() { }

// manipulators

  void set_size(CartanNbr n_Cartans);
  void add(ComplexReductiveGroup& G, CartanNbr cn);
  void add(ComplexReductiveGroup& G, const BitMap& Cartan_classes);

// accessors
  const Cartan_orbit& operator[](CartanNbr cn) const
  { assert(Cartan_index[cn]!=CartanNbr(~0u)); return orbit[Cartan_index[cn]]; }
  CartanNbr Cartan_class(InvolutionNbr i) const
  { return orbit[locate(i)].Cartan_class_nbr; }
  CartanNbr Cartan_class(const TwistedInvolution& tw) const
  { return Cartan_class(nr(tw)); }

  size_t total_size(const BitMap& Cartan_classes) const;

 // make class useful as comparison object, for |std::sort|
  class comparer
  {
    const Cartan_orbits& t;
  public:
  comparer(const Cartan_orbits* o) : t(*o) {}
    bool operator() (InvolutionNbr i, InvolutionNbr j) const; // whether |i<j|
  };
  comparer less() const { return comparer(this); }

}; // |class Cartan_orbits|


} // |namespace involutions|

} // |namespace atlas|

#endif