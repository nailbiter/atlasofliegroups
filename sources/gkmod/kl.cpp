/*!
\file
\brief Implementation of the class KLContext.

  This module contains code for the computation of the Kazhdan-Lusztig
  polynomials for a given block of representations. We have taken the radical
  approach of not using the Bruhat ordering at all, just ordering by length
  instead, and coping with the ensuing appearance of zero polynomials. It
  is expected that the simplification thus achieved will more than outweigh
  the additional polynomials computed.

  The general scheme is fairly similar to the one in Coxeter: there is a
  "KLSupport" structure, that holds the list of primitive pairs that makes it
  possible to read the d_kl list, plus some additional lists that allow for
  a fast primitivization algorithm, for instance; there are two main lists,
  d_kl (filled in for all primitive pairs), and d_mu (filled in only for
  non-zero mu coefficients.)
*/
/*
  This is kl.cpp

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Reductive Lie Groups

  For license information see the LICENSE file
*/

#include "kl.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <string>
#include <algorithm> // for |std::binary_search|

// #include <cassert>
#include "/usr/include/assert.h"
#include <set>
#include <map>
#include <stack>
#include <stdexcept>

#include "hashtable.h"
#include "kl_error.h"
#include "wgraph.h"	// for the |wGraph| function


// extra defs for windows compilation -spc
#ifdef WIN32
#include <iterator>
#endif

/*
  [Fokko's original description, which referred to a slighly older
  version of the computation. Fokko implemented the change from using
  extremal pairs to the slightly large set of primitive pairs, but did
  not change all of the code comments. I am trying to do that now. DV
  8/26/06.]

  This module contains code for the computation of the Kazhdan-Lusztig
  polynomials for a given block of representations. We have taken the radical
  approach of not using the Bruhat ordering at all, just ordering by length
  instead, and coping with the ensuing appearance of zero polynomials. It
  is expected that the simplification thus achieved will more than outweigh
  the additional polynomials computed.

  The general scheme is fairly similar to the one in Coxeter: there is a
  "KLSupport" structure, that holds the list of extremal pairs that makes it
  possible to read the d_kl list, plus some additional lists that allow for
  a fast extremalization algorithm, for instance; there are two main lists,
  d_kl (filled in for all extremal pairs), and d_mu (filled in only for
  non-zero mu coefficients.)
*/

namespace atlas {

namespace kl {

  /*!
\brief Polynomial 0, which is stored as a vector of size 0.
  */
  const KLPol Zero;

  /*! \brief Polynomial 1.q^0. */
  const KLPol One(0,KLCoeff(1)); // Polynomial(d,1) gives 1.q^d.

// we wrap |KLPol| into a class |KLPolEntry| that can be used in a |HashTable|

/* This associates the type |KLStore| as underlying storage type to |KLPol|,
   and adds the methods |hashCode| (hash function) and |!=| (unequality), for
   use by the |HashTable| template.
 */
class KLPolEntry : public KLPol
  {
  public:
    // constructors
    KLPolEntry() : KLPol() {} // default constructor builds zero polynomial
    KLPolEntry(const KLPol& p) : KLPol(p) {} // lift polynomial to this class

    // members required for an Entry parameter to the HashTable template
    typedef KLStore Pooltype;		   // associated storage type
    size_t hashCode(size_t modulus) const; // hash function

    // compare polynomial with one from storage
    bool operator!=(Pooltype::const_reference e) const;

  };


  namespace helper {

  size_t firstAscent(const DescentStatus&,
		     const DescentStatus&, size_t);

  size_t goodAscent(const DescentStatus&,
		    const DescentStatus&, size_t);

  class Thicket;


  typedef hashtable::HashTable<KLPolEntry,KLIndex> KLHashStore;

  class Helper
  : public KLContext
  {

    friend class Thicket;

    // a table for KL polynomial lookup, which is absent in KLContext
    KLHashStore d_hashtable;
    bool d_verbose;

  // Members serving for statistics

    size_t prim_size;            // number of pairs stored in d_prim
    size_t nr_of_prim_nulls;     // number of zeroes suppressed from d_prim

  public:

    // constructors and destructors
    Helper(const KLContext&); // assumes an existing bare-bones |KLContext|

    ~Helper()
    {
      if(d_verbose)
      {
	std::cout << "\nNumber of primitive pairs stored:     "
		  << prim_size << ".\n";
	std::cout << "Number of unrecorded primitive pairs: "
		  << nr_of_prim_nulls << '.' << std::endl;
      }
    }

    //accessors

    size_t firstComplexRecursion(BlockElt y) const;

    size_t firstDirectRecursion(BlockElt y) const;

    std::pair<size_t,size_t> firstImaginaryReal(BlockElt x, BlockElt y) const;

    size_t firstNiceAscent(BlockElt x,BlockElt y) const;

    BlockEltPair inverseCayley(size_t s, BlockElt y) const
    {
      return block().inverseCayley(s,y);
    }

    /* Declare that member function klPol from base class KLContext is also
       considered. This is necessary since it would otherwise be shadowed
       rather than overloaded by a new definition with different signature
       in the Helper class.
    */
    using KLContext::klPol;

    KLPolRef klPol(BlockElt x, BlockElt y,
		   KLRow::const_iterator klv,
		   PrimitiveRow::const_iterator p_begin,
		   PrimitiveRow::const_iterator p_end) const;

    inline bool ascentMu
      (BlockElt x, BlockElt y, size_t s) const;

    inline MuCoeff goodDescentMu
      (BlockElt x, BlockElt y, size_t s) const;

    MuCoeff lengthOneMu(BlockElt x, BlockElt y) const;

    /*!
\brief First coordinate (corresponding to K orbit on G/B) of pair of
integers specifying block element y.
    */
    size_t orbit(BlockElt z) const { return block().x(z); }
    /*!
\brief Second coordinate (corresponding to K^vee orbit on G^vee/B^vee)
of pair of integers specifying block element y.
    */
    size_t dualOrbit(BlockElt z) const { return block().y(z); }

    KLPol lastklPol (std::vector<KLPol>& klv, size_t lwb,
		     const PrimitiveRow& e, BlockElt x); //for computing
							  //last row during
							  //helper constr


    MuCoeff type2Mu(BlockElt x, BlockElt y) const;

    // manipulators
    void rebuild_index() { d_hashtable.reconstruct(); }

    void completePacket(BlockElt y);

    void directRecursion(BlockElt y, size_t s);

    void fill(BlockElt y, bool verbose);

    void fillKLRow(BlockElt y);

    void fillMuRow(BlockElt y);

    void fillThickets(BlockElt y);

    void muCorrection(std::vector<KLPol>& klv,
		      const PrimitiveRow& e,
		      BlockElt y, size_t s);

    void muNewFormula(std::vector<KLPol>& klv, const PrimitiveRow& e,
		      size_t j, BlockElt y, size_t s);

    void newRecursion(BlockElt y);

    void newRecursionRow(std::vector<KLPol> & klv,
		      const PrimitiveRow& e, BlockElt y);

    void recursionRow(std::vector<KLPol> & klv,
		      const PrimitiveRow& e, BlockElt y, size_t s);

    void writeRow(const std::vector<KLPol>& klv,
		  const PrimitiveRow& e, BlockElt y);
  }; // class Helper




    // class Thicket

/*!
\brief Collection of block elements y_j of the same length, differing
by type II real cross actions, and having no other descents.

A pair (y_j, s x y_j) (with s type II real for y_j) is an Edge of the
thicket. Such an s is a descent for both y_j and s x y_j.  The pair is
the image of the Cayley transform (type II imaginary) of a single
element y_0, of length one less.  The first class of KL recursion
relations used in a Thicket is this:

P_{x,y_j} + P_{x,s x y_j} = [formula involving various P_{? , y_0}].

The right hand side terms here are known by induction on y, and recorded in
the recursion data member of the Edge. We can therefore compute P_{x,y} as
soon as we know P_{x,y'} for a single element y' in the thicket. For that
(given x) we find essentially three possibilities:

1) x is equal to y', so P_{x,y'} = 1.

2) There is an s that is a good ascent for x (not in tau(x), and
leading to an x' of length one more than the length of x), but is type
II real for y'.  In this case there is a recursion formula something
like

P_{x,y'} = P_{x' , y'}.

(If s is type II imaginary for x, then there are two terms on the
right.)  In any case the right side is known by downward induction on
x, so finally we know all P_{x,y_j} for y_j in the Thicket.

3) P_{x,y'} = 0.

This computation is carried out in the member function fill().
*/

class Thicket
{
public:

  struct Edge; // subclass defined below
  typedef std::vector<Edge> EdgeList;

private:

  typedef PrimitiveRow::iterator PI;
  typedef KLRow::iterator KLI;

  /*!
    \brief List of elements y_j in Thicket.
  */
  std::vector<BlockElt> d_vertices;

  /*!
    \brief Entry j lists the edges ending at y_j.
  */
  std::vector<EdgeList> d_edges; // surprisingly no incomplete type error here

  /*!
    \brief List of all x that are extremal with respect to some y in Thicket.
  */
  std::vector<BlockElt> d_xlist;

  std::vector<PrimitiveRow> d_extr;
  std::vector<PrimitiveRow> d_prim;
  std::vector<KLRow> d_klr;
  std::vector<PI> d_firstPrim;
  std::vector<KLI> d_firstKL;
  Helper& base;

public:

  // constructors and destructors
  Thicket(Helper&, BlockElt y);

  // methods that simulate a class derived from |Helper|
  size_t rank() const { return base.rank(); }

  BlockElt cross(size_t s, BlockElt y) const
    { return base.cross(s,y); }

  DescentStatus::Value descentValue(size_t s, BlockElt y)
    const
    { return base.descentValue(s,y); }
  const DescentStatus& descent(BlockElt y) const
    { return base.descent(y); }


  KLIndex zero() const { return base.d_zero; }
  KLIndex one() const { return base.d_one; }

  // manipulator access for output
  KLRow& klRow(BlockElt y) { return base.d_kl[y]; } // we're friends
  KLStore& store() { return base.d_store; }

  // this accessor is somewhat different: |y| value is looked up by position
  KLPolRef klPol(BlockElt x, size_t pos) const
  {
    return base.klPol(x,d_vertices[pos],d_firstKL[pos],d_firstPrim[pos],
		      d_prim[pos].end());
  }

  // other accessors

  /*!
    \brief List of |Edge|s ending at y_j.
  */
  const EdgeList& edgeList(size_t j) const { return d_edges[j]; }

  /*!
    \brief List of all x of length strictly less than l(y) that are
    primitive with respect to y_j.

    Primitive means that each descent for y_j is either a descent for x or
    type II imaginary for x.
  */
  const PrimitiveRow& primitiveRow(size_t j) const { return d_prim[j]; }

  size_t nonExtremal(BlockElt x) const;

  /*!
    \brief Number of vertices in Thicket.
  */
  size_t size() const { return d_vertices.size(); }

  /*!
    \brief List of vertices in Thicket.
  */
  const std::vector<BlockElt>& vertices() const { return d_vertices; }

  // manipulators
  bool ascentCompute(BlockElt x, size_t pos);
  void edgeCompute(BlockElt x, size_t pos, const Edge& e);
  void fill();
  void fillXList();

}; // class Thicket

    /*!
\brief Pair (y , s x y) (with s type II real) in a Thicket.

    */
struct Thicket::Edge {

    /*!
\brief Index in d_vertices of the block element at which the edge begins.
    */
    size_t source;

    /*!
\brief Index in d_vertices of the block element at which the edge ends.
    */
    size_t y;

    /*!
\brief Number of a simple root s, assumed to be type II real for y.
    */
    size_t s;

    /*!
\brief List of formulas for P_{x_i,y} + P_{x_i,source}, for i in a
list of elements primitive with respect to some y' in the Thicket.
    */
    std::vector<KLPol> recursion;
    // constructors and destructors
    Edge() {}
    Edge(BlockElt x, BlockElt y,
	 size_t s, const std::vector<KLPol>& r)
      :source(x), y(y), s(s), recursion(r) {}
}; // struct Thicket::Edge

class ThicketIterator {

  private:
    typedef Thicket::EdgeList EdgeList;

    std::stack<size_t> d_stack;
    BitMap d_done;
    EdgeList::const_iterator d_current;
    EdgeList::const_iterator d_currentEnd;
    size_t d_pos;

    const Thicket* d_thicket;

  public:

    // constructors and destructors
    ThicketIterator(const Thicket&, size_t);

    // accessors
    bool operator() () const {
      return not d_stack.empty();
    }

    size_t operator* () const { // current vertex index, or size() at end
      return d_pos;
    }

    const Thicket::Edge& edge() const { // current edge
      return *d_current;
    }

    // manipulators
    ThicketIterator& operator++();

}; // class ThicketIterator

} // namespace helper
} // namespace kl

/*****************************************************************************

        Chapter I -- Methods of the KLContext and KLPolEntry classes.

 *****************************************************************************/

/* methods of KLContext */


namespace kl {

KLContext::KLContext(const Block_base& b)
  : klsupport::KLSupport(b) // construct unfilled support object from block
  , fill_limit(0)
  , d_prim()
  , d_kl()
  , d_mu()
  , d_store(2)
  , d_zero(0)
  , d_one(1)
{
  d_store[d_zero]=Zero;
  d_store[d_one]=One;

}

/******** copy, assignment and swap ******************************************/


/*!
  \brief Copy constructor. Used only to copy-construct base for Helper class
*/
KLContext::KLContext(const KLContext& other)
  : klsupport::KLSupport(other)
  , fill_limit(other.fill_limit)
  , d_prim(other.d_prim)
  , d_kl(other.d_kl)
  , d_mu(other.d_mu)
  , d_store(other.d_store)
  , d_zero(other.d_zero)
  , d_one(other.d_one)
{}



void KLContext::swap(KLContext& other) // used to extract base out of Helper
{
  klsupport::KLSupport::swap(other); // swap base (support) objects
  std::swap(fill_limit,other.fill_limit);

  d_prim.swap(other.d_prim);

  d_kl.swap(other.d_kl);
  d_mu.swap(other.d_mu);

  d_store.swap(other.d_store);  // this puts the Helper store into base object
  std::swap(d_zero,other.d_zero);
  std::swap(d_one,other.d_one);
}

// If table extension fails, we need to recover the old part of the tables
void KLContext::partial_swap(KLContext& other)
{
  // this is only called both to copy old tables into the helper object |other|,
  // and in case of an overflow to recover those old tables form |other|
  klsupport::KLSupport::swap(other); // swap base (support) objects

  d_prim.swap(other.d_prim);
  d_kl.swap(other.d_kl);
  d_mu.swap(other.d_mu);
  d_store.swap(other.d_store);
  std::swap(d_zero,other.d_zero);
  std::swap(d_one,other.d_one);

  if (other.fill_limit<fill_limit) // we are initially filling helper object
  {
    other.fill_limit=fill_limit; // for fill limit, copy to other but keep ours
    d_store.reserve(other.d_store.size()); // a stupid way to record old size
  }
  else // then we are restoring after a throw; |other| will soon disappear
  {
    d_prim.resize(fill_limit); // truncate, freeing occupied memory
    d_kl.resize(fill_limit);
    d_mu.resize(fill_limit);
    d_store.resize(other.d_store.capacity()); // truncate to old size
  }

}

/******** accessors **********************************************************/


/*!
  \brief Returns the Kazhdan-Lusztig-Vogan polynomial P_{x,y}

  Precondition: x and y are smaller than size();

  Explanation: since d_store holds all polynomials for primitive
  pairs (x,y), this is basically a lookup function. While x is not
  primitive w.r.t. y, it moves x up, using the "easy" induction
  relations. At that point, we look x up in the primitive list for
  y. If it is found, we get a pointer to the result.  If it is not
  found, the polynomial is zero.
*/
KLPolRef KLContext::klPol(BlockElt x, BlockElt y) const
{
  const PrimitiveRow& pr = d_prim[y];
  const KLRow& klr = d_kl[y];

  x=primitivize(x,descentSet(y));
  if (x>y) return Zero; // includes case x==blocks::UndefBlock
  PrimitiveRow::const_iterator xptr =
    std::lower_bound(pr.begin(),pr.end(),x);
  if (xptr == pr.end() or *xptr != x) // not found
    return Zero;
  return d_store[klr[xptr - pr.begin()]];
}


/*!
  \brief Returns mu(x,y).

  Explanation: it is guaranteed that all the x'es such that mu(x,y) != 0
  occur in d_mu[y] (and in fact, that only those occur.) So it is a simple
  matter of looking up x.
*/
MuCoeff KLContext::mu(BlockElt x, BlockElt y) const
{
  const MuRow& mr = d_mu[y];

  std::vector<BlockElt>::const_iterator xloc=
    std::lower_bound(mr.first.begin(),mr.first.end(),x);

  if (xloc==mr.first.end() or *xloc!=x)
    return 0; // x not found in mr

  return mr.second[xloc-mr.first.begin()];
}

/* The following two methods were moved here from the Helper class, since
   they turn out to be useful even when no longer constructing the KLContext
*/

/*!
  \brief Puts in e the list of all x extremal w.r.t. y.

  Explanation: this means that either x = y, or length(x) < length(y),
  and every descent for y is a descent for x.
*/
void
KLContext::makeExtremalRow(PrimitiveRow& e, BlockElt y)
  const
{
  BitMap b(size());
  b.fill(0,lengthLess(length(y))); // start with all elements < y in length
  b.insert(y);                     // and y itself

  // extremalize (filter out those that are not extremal)
  extremalize(b,descentSet(y));

  // copy from bitset b to list e
  e.reserve(e.size()+b.size()); // ensure tight fit after copy
  std::copy(b.begin(),b.end(),back_inserter(e));

}


/*!
  \brief Puts in e the list of all x primitive w.r.t. y.

  Explanation: this means that either x = y, or length(x) < length(y),
  and every descent for y is either a descent, or an imaginary type II
  ascent for x.
*/
void
KLContext::makePrimitiveRow(PrimitiveRow& e, BlockElt y)
  const
{
  BitMap b(size());
  b.fill(0,lengthLess(length(y))); // start with all elements < y in length
  b.insert(y);                     // and y itself

  // primitivize (filter out those that are not primitive)
  primitivize(b,descentSet(y));
  // copy to list
  e.reserve(e.size()+b.size()); // ensure tight fit after copy
  std::copy(b.begin(),b.end(),back_inserter(e));

}


/******** manipulators *******************************************************/

/*!
  \brief Fills (or extends) the KL- and mu-lists.

  Explanation: this is the main function in this module; all the work is
  deferred to the Helper class.
*/
void KLContext::fill(BlockElt y, bool verbose)
{

  if (y<fill_limit)
    return; // tables present already sufficiently large for |y|

#ifndef VERBOSE
  verbose=false; // if compiled for silence, force this variable
#endif

  if (verbose)
    std::cerr << "computing Kazhdan-Lusztig polynomials ..." << std::endl;

  helper::Helper help(*this); // make helper, with empty base KLContext
  if (fill_limit>0)
  {
    partial_swap(help);   // move previously computed tables into helper
    help.rebuild_index(); // since we just swapped the referred-to |KLStore|
  }

  try
  {
    help.fill(y,verbose); // takes care of filling embedded |KLSupport| too
    swap(help); // swap base object, including the |KLSupport| and |KLStore|

    fill_limit = y+1;

    if (verbose)
      std::cerr << "done" << std::endl;
  }
  catch (std::bad_alloc)
  { // roll back, and transform failed allocation into MemoryOverflow
    std::cerr << "\n memory full, KL computation abondoned." << std::endl;
    partial_swap(help); // restore (only) the previous contents from helper
    throw error::MemoryOverflow();
  }

}

BitMap KLContext::primMap (BlockElt y) const
{
  BitMap b(size()); // block-size bitmap

  // start with all elements < y in length
  b.fill(0,lengthLess(length(y)));
  b.insert(y);   // and y itself

  // primitivize (filter out those that are not primitive)
  primitivize(b,descentSet(y));

  // now b holds a bitmap indicating primitive elements for y

  // our result will be a bitmap of that size
  BitMap result (b.size()); // initiallly all bits are cleared

 // the list of primitive elements with nonzero polynomials at y
  const PrimitiveRow& row=d_prim[y];

  // traverse |b|, and for its elements that occur in, set bits in |result|

  size_t position=0; // position among set bits in b (avoids using b.position)
  size_t j=0; // index into row;
  for (BitMap::iterator it=b.begin(); it(); ++position,++it)
    if (*it==row[j]) // look if |it| points to current element of row
    {
      result.insert(position); ++j; // record position and advance in row
      if (j==row.size()) break;     // stop when row is exhausted
    }

  return result;
}


/* methods of KLPolEntry */


/*!
  \brief calculate a hash value in [0,modulus[, where modulus is a power of 2

  The function is in fact evaluation of the polynomial (with coefficients
  interpreted in Z) at the point 2^21+2^13+2^8+2^5+1=2105633, which can be
  calculated quickly (without multiplications) and which gives a good spread
  (which is not the case if 2105633 is replaced by a small number, because
  the evaluation values will not grow fast enough for low degree
  polynomials!).

*/
inline size_t KLPolEntry::hashCode(size_t modulus) const
{ const KLPol& P=*this;
  if (P.isZero()) return 0;
  polynomials::Degree i=P.degree();
  size_t h=P[i]; // start with leading coefficient
  while (i-->0) h= ((h<<21)+(h<<13)+(h<<8)+(h<<5)+h+P[i]) & (modulus-1);
  return h;
}

bool KLPolEntry::operator!=(KLPolEntry::Pooltype::const_reference e) const
{
  if (degree()!=e.degree()) return true;
  if (isZero()) return false; // since degrees match
  for (polynomials::Degree i=0; i<=degree(); ++i)
    if ((*this)[i]!=e[i]) return true;
  return false; // no difference found
}

} // namespace kl


/*****************************************************************************

        Chapter II -- Methods of the Helper class.

 *****************************************************************************/

namespace kl {
  namespace helper {

    // main constructor

Helper::Helper(const KLContext& kl)
  : KLContext(kl.block())  // re-construct empty KLContext base object
  , d_hashtable(d_store) // hash table identifies helper base object's d_store
  , d_verbose(false)
  , prim_size(0), nr_of_prim_nulls(0)
{
}

/******** accessors **********************************************************/



/*!
  \brief Returns the first descent generator that is complex

  Explanation: those are the ones that give an easy recursion formula for the
  K-L basis element. Explicitly, we search for a generator |s| such that
  |descentValue(s,y)| is |DescentStatus::ComplexDescent|. If no such generator exists, we return |rank()|.
*/
size_t Helper::firstComplexRecursion(BlockElt y) const
{
  const DescentStatus& d = descent(y);

  for (size_t s = 0; s < rank(); ++s) {
    //    DescentStatus::Value v = d[s];
    if(d[s]==DescentStatus::ComplexDescent)
       return s;
  }

  return rank();
}


/*!
  \brief Returns the first descent generator that is not real type II

  Explanation: those are the ones that give a direct recursion formula for the
  K-L basis element. Explicitly, we search for a generator |s| such that
  |descentValue(s,y)| is either |DescentStatus::ComplexDescent| or
  |DescentStatus::RealTypeI|. If no such generator exists, we return |rank()|.
*/
size_t Helper::firstDirectRecursion(BlockElt y) const
{
  const DescentStatus& d = descent(y);

  for (size_t s = 0; s < rank(); ++s) {
    DescentStatus::Value v = d[s];
    if (DescentStatus::isDirectRecursion(v))
      return s;
  }

  return rank();
}//firstDirectRecursion

/*!
  \brief Returns the first pair (s,t) such that
  1) (s,t) is (rn,r2) for y;
  2) (s,t) is (i1,ic) for x;
  3) (s,t) is (i1,i1/2) for s.x
  or (rank,rank) if no such pair exists. Such a pair is used to compute
  P_{x,y}+P_{sx,y} using s, then P_{sx,y} using t, and so P_{x,y}.
*/

    std::pair<size_t,size_t> Helper::firstImaginaryReal(BlockElt x,
							BlockElt y) const
{
  const DescentStatus& dx = descent(x);
  const DescentStatus& dy = descent(y);

  for (size_t s = 0; s < rank(); ++s) {
    DescentStatus::Value vxs = dx[s];
    DescentStatus::Value vys = dy[s];
    if (vys==DescentStatus::RealNonparity and vxs==DescentStatus::ImaginaryTypeI)
      {
	BlockElt x1 = cross(s,x);
	const DescentStatus& dx1 = descent(x1);
	for (size_t t = 0; t < rank(); ++t)
	  {
	    DescentStatus::Value vyt = dy[t];
	    DescentStatus::Value vx1t = dx1[t];
	    if ((vyt==DescentStatus::RealTypeI or
		 vyt==DescentStatus::RealTypeII) and
		(vx1t==DescentStatus::ImaginaryTypeI or
		 vx1t==DescentStatus::ImaginaryTypeII))
	      return std::make_pair(s,t);}
	  }
  }//loop in s
    return std::make_pair(rank(),rank());
  }//firstImaginaryReal

/*!

  \brief Returns the first complex ascent, or compact imaginary
  generator for x that is
  real nonparity for y. Explanation: those are the ones that give the best
  new recursion formula for the K-L basis element. Explicitly, we
  search for a generator |s| such that |descentValue(s,y)| is
  |DescentStatus::RealNonparity|, and |descentValue(s,x)| is either
  |DescentStatus::ComplexAscent| or |DescentStatus::CompactImaginary.
  If no such generator exists, we return |rank()|.
*/

    size_t Helper::firstNiceAscent(BlockElt x,BlockElt y) const
{
  const DescentStatus& dx = descent(x);
  const DescentStatus& dy = descent(y);

  for (size_t s = 0; s < rank(); ++s) {
    DescentStatus::Value vx = dx[s];
    DescentStatus::Value vy = dy[s];
   if ((vy==DescentStatus::RealNonparity) and
       ((vx==DescentStatus::ComplexAscent) or
	(vx==DescentStatus::ImaginaryCompact) or
	(vx==DescentStatus::ImaginaryTypeII))
       ) return s;

// (vx==DescentStatus::ImaginaryTypeII))) //SHOULD INCLUDE THIS ONE!

  }

  return rank();
}


/*!
  \brief Returns the Kazhdan-Lusztig polynomial for x corresponding to
  the given row.

  Precondition: |klv| holds the tail of the set of primitive Kazhdan-Lusztig
  polynomials for |y|, enough to find the required one by elementary lookup;
  |[p_begin,p_end[| is the corresponding range of primitive elements.

  Algorithm: primitivize |x| with respect to the descents in |y|; if a real
  nonparity situation is encountered, return |Zero|; otherwise look up the
  primitive |x| in the range and return the corresponding element from |klv|
*/
KLPolRef Helper::klPol(BlockElt x, BlockElt y,
		       KLRow::const_iterator klv,
		       PrimitiveRow::const_iterator p_begin,
		       PrimitiveRow::const_iterator p_end) const
{
  BlockElt xp = primitivize(x,descentSet(y));

  if (xp>y) return Zero; // includes case |xp==blocks::UndefBlock|
  PrimitiveRow::const_iterator xptr =
    std::lower_bound(p_begin,p_end,xp);
  if (xptr == p_end or *xptr != xp) return Zero;
  return d_store[klv[xptr-p_begin]];
}


/*!
  \brief Computes whether |mu(x,y)==1| in a good ascent situation

  Preconditions: $l(y)>0$; $l(x)=l(y)-1$; $s$ is an ascent for $x$ w.r.t. $y$

  Explanation: this is the situation where |mu(x,y)| is directly expressible.
  The point is that |x| will ascend to an element of the same length as |y|,
  so that the corresponding K-L polynomial is zero unless |x| ascends to |y|.
*/
inline bool
Helper::ascentMu(BlockElt x, BlockElt y, size_t s) const
{
  switch (descentValue(s,x)) {
  case DescentStatus::ComplexAscent:
    return cross(s,x) == y;
  case DescentStatus::RealNonparity:
    return false;
  case DescentStatus::ImaginaryTypeI:
    return cayley(s,x).first == y;
  case DescentStatus::ImaginaryTypeII:
    {
      BlockEltPair x1 = cayley(s,x);
      // mu(x,y) = mu(x1.first,y) + mu(x1.second,y)
      return x1.first == y or x1.second == y;
    }
  default: // this cannot happen
    assert(false);
    return false; // keep compiler happy
  }
}


/*!
  \brief Gets |mu(x,y)| by a good descent recursion.

  Precondition: $l(y)>0$; $l(x)=l(y)-1$; all previous mu-rows have been filled
  in; |s| is a good descent for |y|;

  Explanation: a good descent for |y| is a descent that is neither real type
  II nor imaginary compact (so it is either a complex or real type I); these
  are the cases where |mu(x,y)| is directly expressed by recursion.
*/
inline MuCoeff
Helper::goodDescentMu(BlockElt x, BlockElt y, size_t s) const
{
  BlockElt y1;

  if (descentValue(s,y) == DescentStatus::ComplexDescent)
    y1 = cross(s,y);
  else // s is real type I for y, chooise one of double-valued inverse Cayley
    y1 = inverseCayley(s,y).first;

  switch (descentValue(s,x))
  {
  case DescentStatus::ImaginaryCompact:
    return MuCoeff(0);
  case DescentStatus::ComplexDescent:
      return mu(cross(s,x),y1);
  case DescentStatus::RealTypeI:
    {
      BlockEltPair x1 = inverseCayley(s,x);
      return mu(x1.first,y1)+mu(x1.second,y1);
    }
  case DescentStatus::RealTypeII:
    return mu(inverseCayley(s,x).first,y1);

  default: // this cannot happen
    assert(false);
    return MuCoeff(0); // keep compiler happy
  }
}


/*!
  \brief Computes $\mu(x,y)$ in the special case that $l(y)-l(x) = 1$.

  Preconditions: $l(y) > 0$; $l(x) = l(y)-1$; the $\mu$-rows for all $y$
  of smaller lengths have already been filled in.

  Explanation: these are the $\mu$-values that can come up in possibly
  non-extremal situations. The value can be obtaind simply as the constant
  term of |klPol(x,y)|, but for efficiency reasons we handle some easy cases
  directly, avoiding the cost of calling |klPol|. In fact in all the cases
  these values of $\mu$ can be, and used to be, computed recursively by
  formulas using only other $\mu$-values of the same kind.
*/
MuCoeff Helper::lengthOneMu(BlockElt x, BlockElt y) const
{
  // look if x has any ascents that are descents for y
  RankFlags ascents=descentSet(y); ascents.andnot(descentSet(x));
  if (ascents.any())
    return ascentMu(x,y,ascents.firstBit()) ? MuCoeff(1) : MuCoeff(0);

  /* Doing the following case separately costs more time than it gains

  // if we get here, x is extremal w.r.t. y
  s = firstDirectRecursion(y);
  if (s != rank()) // the answer is another $\mu$
    return goodDescentMu(x,y,s);
  */

  // Now that the easy cases are gone, just lookup the KL polynomial
  KLPolRef p=klPol(x,y);
  return p.isZero() ? MuCoeff(0) : p[0];

  // the final case used to be: return type2Mu(x,y);

}

/*!

   \brief Looks up klPol(x,y) in klv during the helper construction of row y.

   Precondition: klv[i] contains the polynomials klPol(e[i],y) whenever
   e[i]>= x, which implies i>=lwb (slots below klv[lwb] are not yet filled).
   If x==e[i] for some i then return klv[i].
   If there is no such i then x has an ascent that is descent for y that
   allows expression in terms of polynomials that \emph{can} be found in klv.
*/
KLPol Helper::lastklPol (std::vector<KLPol>& klv, size_t lwb,
			 const PrimitiveRow& e, BlockElt x)
{
  BlockElt y = e.back();
  if (x>y) return Zero; // includes case x==blocks::UndefBlock //
			// should be in |primitivize|
  x = primitivize(x,descentSet(y));
  if (x>y) return Zero; // includes case x==blocks::UndefBlock
  PrimitiveRow::const_iterator xptr =
    std::lower_bound(e.begin()+lwb,e.end(),x);
  if ((xptr != e.end()) and (*xptr == x)) //  found
    return klv[xptr - e.begin()];
//now x is primitive but not extremal.

  size_t s = firstAscent(descent(x),descent(y),rank());
  if (s == rank()) return Zero;
  assert(descentValue(s,x)==DescentStatus::ImaginaryTypeII);
  BlockEltPair x1 = cayley(s,x); // must be imaginary type II
  KLPol pol = lastklPol(klv,lwb,e,x1.first);
  pol.safeAdd(lastklPol(klv,lwb,e,x1.second));

  return pol;
}//lastklPol


/*!
  \brief Gets mu(x,y) by type II recursion.

  This code is currently unused (see lengthOneMu above).

  Precondition: length(y) > 0; length(x) = length(y)-1; all previous
  mu-rows have been filled in; x is extremal w.r.t. y, and all descents
  of y are real type II;

  Explanation: in this situation, we have recursion formulas that will
  yield mu(x,y)+mu(x,s.y). It is known that proceeding in this way we must
  end up with a y' for which x is not extremal.

  Algorithm: we keep a stack of recursion formulas of the above kind, walking
  through the orbit of y under the simple descents, until we reach a y
  that (a) has a non-type-II descent, or (b) for which x is non-extremal.
*/
MuCoeff Helper::type2Mu(BlockElt x, BlockElt y) const
{
  std::map<BlockElt,size_t> rel;
  std::stack<BlockElt> toDo;

  rel.insert(std::make_pair(y,rank()));
  toDo.push(y);

  size_t s;
  BlockElt y1 = y;
  MuCoeff mu = 0;

  while (not toDo.empty()) {
    y1 = toDo.top();
    toDo.pop();
    if (y1 < y) { // mu(x,y1) can be gotten recursively
      mu = KLContext::mu(x,y1);
      goto unwind;
    }
    s = firstAscent(descent(x),descent(y1),rank());
    if (s != rank()) { // done
      mu = ascentMu(x,y1,s);
      goto unwind;
    }
    for (s = 0; s < rank(); ++s)
    {
      DescentStatus::Value v = descentValue(s,y1);
      if (not DescentStatus::isDescent(v))
	continue;
      if (DescentStatus::isDirectRecursion(v)) {
	mu = goodDescentMu(x,y1,s);
	goto unwind;
      }
      // at this point s is either real type II or imaginary compact
      if (v == DescentStatus::ImaginaryCompact)
	continue;
      // at this point, there is no hope to resolve the situation at y1
      {
	BlockElt y2 = cross(s,y1);
	if (rel.insert(std::make_pair(y2,s)).second) // new element
	  toDo.push(y2);
      }
    }
  }

 unwind:
  while (y1 != y) {
    std::map<BlockElt,size_t>::iterator i = rel.find(y1);
    s = i->second;
    y1 = cross(s,y1);
    // goodDescent() is mu + mu(x,y2)
    mu = goodDescentMu(x,y1,s) - mu;
  }

  return mu;
}

/******** manipulators *******************************************************/

/*!
  \brief Finishes the filling of the R-packet starting at y.

  Precondition: all the rows in the packet capable of a direct recursion
  are filled; at least one row is of this form;

  Explanation: what we do here, is completing the rows that can be completed
  from the already filled ones, using real type II recursions. Whatever is left
  will constitute "thickets", and will be dealt with by another function.

  Algorithm: we traverse the set of y1 in the packet that can be obtained from
  one of the filled ones through real type II cross-actions.
*/
void Helper::completePacket(BlockElt y)
{
  std::pair<BlockElt,BlockElt> packet = block().R_packet(y);
  assert (packet.first==y);
  std::stack<BlockElt> filled;
  std::set<BlockElt> empty;

  for (BlockElt y1 = packet.first; y1<packet.second; ++y1) {
    if (d_kl[y1].size() != 0)
      filled.push(y1);
    else
      empty.insert(y1);
  }

  while (not filled.empty())
  {
    BlockElt y1 = filled.top();
    filled.pop();

    for (size_t s = 0; s < rank(); ++s) {
     if (descentValue(s,y1) != DescentStatus::RealTypeII)
	continue;
      BlockElt y2 = cross(s,y1);
      std::set<BlockElt>::iterator i = empty.find(y2);
      if (i != empty.end()) { // found a new row
 	// fill row y2
	std::vector<KLPol> klv;
	PrimitiveRow e;
	makeExtremalRow(e,y2);
	recursionRow(klv,e,y2,s);
	// klv[j] is P_{x,y2}+P_{x,y1}, for x = e[j]
	for (size_t j = 0; j < klv.size(); ++j) {
	  BlockElt x = e[j];
	  try {
	    klv[j].safeSubtract(klPol(x,y1));
	  }
	  catch (error::NumericUnderflow& e){
	    throw kl_error::KLError(x,y1,__LINE__,
				    static_cast<const KLContext&>(*this));
	  }
	}
	// write out row
	writeRow(klv,e,y2);
	// update empty and filled
	empty.erase(i);
	filled.push(y2);
      }
    }
  }

  if (not empty.empty()) // we are not done
    fillThickets(y);

}


/*!
  \brief Fills in the row for y using a direct recursion.

  Precondition: s is either a complex, or a real type I descent generator for
  y.

  The real work is done by the recursionRow function, that will be used also
  in the real type II descents.
*/
void Helper::directRecursion(BlockElt y, size_t s)
{
  std::vector<KLPol> klv;
  PrimitiveRow e;

  // put result of recursion formula in klv
  makeExtremalRow(e,y);
  recursionRow(klv,e,y,s);

  // write result
  writeRow(klv,e,y);

}


/*!
  \brief Dispatches the work of filling the KL- and mu-lists.

  The current implementation blindly assumes that the tables are empty
  initially. While this is true in the way it is now used, it should really be
  rewritten to take into account the possibility of initial |fill_limit>0|
*/
void Helper::fill(BlockElt last_y, bool verbose)
{
  // make sure the support (base of base) is filled
  klsupport::KLSupport::fill();

  // resize the outer lists to the block size
  d_prim.resize(size());
  d_kl.resize(size());
  d_mu.resize(size());

  // fill the lists
  size_t minLength = length(0);
  size_t maxLength = length(last_y<size() ? last_y : size()-1);

  // do the minimal length cases; they come first in the enumeration
  for (BlockElt y = 0; y < lengthLess(minLength+1); ++y) {
    d_prim[y].push_back(y); // singleton list for this row
    ++prim_size;
    // the K-L polynomial is 1
    d_kl[y].push_back(d_one);
    // there are no mu-coefficients
  }

  //set timers for KL computation
  d_verbose=verbose; // inform our destructor of |verbose| setting
  std::time_t time0;
  std::time(&time0);
  std::time_t time;

  std::ifstream statm("/proc/self/statm"); // open file for memory status

  // do the other cases
  for (size_t l=minLength+1; l<=maxLength; ++l)
  {
    BlockElt y_limit = l<maxLength ? lengthLess(l+1) : last_y+1;
    for (BlockElt y=lengthLess(l); y<y_limit; ++y)
    {
      if (verbose)
	std::cerr << y << "\r";

      try
      {
	fillKLRow(y);
      }
      catch (kl_error::KLError& e)
      {
	e("error: negative coefficient in k-l construction");
	throw; // quick fix to stop computation; catch should be further out
      }
      fillMuRow(y);
    }

    // now length |l| is completed
    if (verbose)
    {
      size_t p_capacity // currently used memory for polynomials storage
	=d_hashtable.capacity()*sizeof(KLIndex)
	+ d_store.capacity()*sizeof(KLPol);
      for (size_t i=0; i<d_store.size(); ++i)
	p_capacity+= (d_store[i].degree()+1)*sizeof(KLCoeff);

      std::time(&time);
      double deltaTime = difftime(time, time0);
      std::cerr << "t="    << std::setw(5) << deltaTime
		<< "s. l=" << std::setw(3) << l // completed length
		<< ", y="  << std::setw(6)
		<< lengthLess(l+1)-1 // last y value done
		<< ", polys:"  << std::setw(11) << d_store.size()
		<< ", pmem:" << std::setw(11) << p_capacity
		<< ", mat:"  << std::setw(11) << prim_size
		<<  std::endl;

      unsigned int size, resident,share,text,lib,data;
      statm.seekg(0,std::ios_base::beg);
      statm >> size >> resident >> share >> text >> lib >> data;
      std::cerr << "Current data size " << data*4 << "kB (" << data*4096
		<< "), resident " << resident*4 << "kB, total "
		<< size*4 << "kB.\n";
    }

  } // for (l=min_length+1; l<=max_Length; ++l)

  if (verbose)
  {
    std::time(&time);
    double deltaTime = difftime(time, time0);
    std::cerr << std::endl;
    std::cerr << "Total elapsed time = " << deltaTime << "s." << std::endl;
    std::cerr << d_store.size() << " polynomials, "
	      << prim_size << " matrix entries."<< std::endl;

    std::cerr << std::endl;
  }

}


/*!
  \brief Fills in the row for y in the KL-table.

  Precondition: all lower rows have been filled; y is of length > 0;
  R-packets are consecutively numbered;

  Explanation: this function actually fills out the whole R-packet to
  which y belongs (unless it returns immediately). This is done in two
  stages: first, we fill the rows for which there is a direct recursion
  relation. The second stage, if any, is forwarded to the completePacket
  function.
*/
void Helper::fillKLRow(BlockElt y)
{
  if (d_kl[y].size()) // row has already been filled
    return;

  //  std::pair<BlockElt,BlockElt> packet = block().R_packet(y);
  //  assert (packet.first==y);
  //  bool done = true;

  // fill in the direct recursions in the R-packet
  //  for (BlockElt y1 = packet.first; y1<packet.second; ++y1) {
  size_t s = firstDirectRecursion(y);
  //  size_t s = firstComplexRecursion(y);
  if (s != rank())  // direct recursion
      directRecursion(y,s);
    else
    newRecursion(y); //roots rn for y, C+ (or ic?) for x ETC.
  // deal with the others
  //  if (not done)
    //  completePacket(y);

}


/*!
  \brief Fills in the row for y in the mu-table.

  Precondition: the row for y in the KL-table has been filled; length(y) > 0;

  Explanation: for the elements of length < length(y) - 1, mu(x,y) can
  be non-zero only if x is extremal w.r.t. y; so we run through d_kl[y],
  and look at the cases where the polynomial is of degree (1/2)(l(y)-l(x)-1)
  (the biggest possible). For the elements of colength 1, in the classical
  case we always had mu(x,y)=1. Here that's not true anymore, zero might
  be a possibility, and also larger values I believe; but at any rate the
  mu-values can be computed in terms of other mu-values.

  NOTE: we are not using the hasse-list here, although it is probably a
  good idea to compute that; that will reduce the mu-computation.
*/
void Helper::fillMuRow(BlockElt y)
{
  const PrimitiveRow& e = d_prim[y]; // list of nonzero polynomials

  size_t ly = length(y);
  if (ly==0) // we are in fact never called for |y| values of length 0
    return;  // but this is prudent, since next loop would fail for |ly==0|

  PrimitiveRow::const_iterator start= e.begin();
  // traverse lengths of opposite parity, up to ly-3
  for (size_t lx=(ly-1)%2,d = (ly-1)/2; d>0; --d,lx+=2) {// d=(ly-1-lx)/2

    PrimitiveRow::const_iterator stop =
      std::lower_bound(start,e.end(),lengthLess(lx+1));
    for (start= std::lower_bound(start,stop,lengthLess(lx));
	 start<stop; ++start) {
      BlockElt x = *start;
      KLIndex klp = d_kl[y][start-e.begin()];

      KLPolRef p=d_store[klp];
      if (p.degree()== d) { // then we have found a mu-coefficient for x
	d_mu[y].first.push_back(x);
	d_mu[y].second.push_back(p[d]);
      }
    }
  }

  // do cases of length ly-1
  BlockElt x_begin = lengthLess(ly-1);
  BlockElt x_end = lengthLess(ly);

  for (BlockElt x = x_begin; x < x_end; ++x) {
    MuCoeff mu = lengthOneMu(x,y);
    if (mu!=MuCoeff(0)) {
      d_mu[y].first.push_back(x);
      d_mu[y].second.push_back(mu);
    }
  }

}


/*!
  \brief Finishes the filling of the R-packet starting at y.

  Precondition: all the rows in the packet that are not part of a "thicket"
  have already been filled.

  Explanation: a thicket is a connected component in the graph whose edges are
  formed by real type II cross-actions, all of whose elements have _only_ real
  type II descents (for instance, it happens quite often that the principal
  series representations are in a thicket.) Then we must use the "structural
  fact" in Lemma 6.2 of David's Park City notes: for each x lower than y,
  there is an element y' of the thicket for which x has an ascent (or if there
  is no such y', the K-L polynomial is zero.)

  Algorithm: (a) for each y' in the R-packet that has not already been filled,
  we determine the thicket of y' via a traversal algorithm, as usual (b) we
  fill in all the P_{x,y} in that thicket by a downwards recursion. In order
  to do that, we put the union of all the x'es in the extremal lists in a
  common ordered list. Then we move down the list; for each x, by assumption
  there is an y' in the thicket for which it is not extremal, and starting
  from there we can fill in P_{x,y} in all the rows where it needs to be
  filled.

  NOTE: thickets are always small, at the very worst a few hundred elements,
  so we don't have to worry about efficiency here.

  NOTE: perhaps it might be expected that there is usually (always?) a "large"
  element in the thicket, that would provide ascents for all x'es. So it might
  be worthwhile to sort the elements in the thicket in order of number of
  descents.
*/
void Helper::fillThickets(BlockElt y)
{
  std::pair<BlockElt,BlockElt> packet = block().R_packet(y);
  assert (packet.first==y);

  for (BlockElt y1 = packet.first; y1<packet.second; ++y1)
    if (d_kl[y1].size() == 0) {
      Thicket thicket(*this,y1);
      thicket.fill();
    }

}


/*!
  \brief Subtracts from all polynomials in |klv| the correcting terms in the
  K-L recursion.

  Precondtion: |klv| already contains, for all $x$ that are primitive w.r.t.
  |y| in increasing order, the terms in $P_{x,y}$ corresponding to
  $c_s.c_{y1}$, whery |y1| is $s.y$ if |s| is a complex descent, and |y1| is
  an inverse Cayley transform of |y| if |s| is real type I or II.
  The mu-table and KL-table have been filled in for elements of length < l(y).

  Explanation: the recursion formula is of the form:
  $$
    lhs = c_s.c_{y1} - \sum_{z} mu(z,y1)c_z
  $$
  where |z| runs over the elements $< y1$ such that |s| is a descent for |z|.
  Here $lhs$ stands for $c_y$ when |s| is a complex descent or real type I for
  |y|, and for $c_{y}+c_{s.y}$ when |s| is real type II; however it plays no
  part in this function that only subtracts $\mu$-terms.

  We construct a loop over |z| first, before traversing |klv| (the test for
  $z<y1$ is absent, but $\mu(z,y1)\neq0$ implies $z\leq y1$, and $z=y1$ will
  be rejected by the descent condition). The chosen loop order allows fetching
  $\mu(z,y1)$ only once, and terminating the scan of |klv| once its values |x|
  become too large to produce a non-zero $P_{x,z}$.
*/
void Helper::muCorrection(std::vector<KLPol>& klv,
			  const PrimitiveRow& e,
			  BlockElt y, size_t s)
{
  BlockElt y1;

  if (descentValue(s,y) == DescentStatus::ComplexDescent) {
    y1 = cross(s,y);
  } else { // s is real for y
    y1 = inverseCayley(s,y).first;
  }

  const MuRow& mrow = d_mu[y1];
  size_t l_y = length(y);

  for (size_t i = 0; i < mrow.first.size(); ++i) {

    BlockElt z = mrow.first[i];
    size_t l_z = length(z);

    DescentStatus::Value v = descentValue(s,z);
    if (not DescentStatus::isDescent(v))
      continue;

    MuCoeff mu = mrow.second[i]; // mu!=MuCoeff(0)

    polynomials::Degree d = (l_y-l_z)/2; // power of q used in the loops below

    if (mu==MuCoeff(1)) // avoid useless multiplication by 1 if possible

      for (size_t j = 0; j < e.size(); ++j) {
	BlockElt x = e[j];
	if (length(x) > l_z) break; // once reached, no more terms for |z|

	KLPolRef pol = klPol(x,z);
	try {
	  klv[j].safeSubtract(pol,d); // subtract q^d.P_{x,z} from klv[j]
	}
	catch (error::NumericUnderflow& e){
	  throw kl_error::KLError(x,y,__LINE__,
				  static_cast<const KLContext&>(*this));
	}
      } // for (j)

    else // mu!=MuCoeff(1)

      for (size_t j = 0; j < e.size(); ++j) {
	BlockElt x = e[j];
	if (length(x) > l_z) break; // once reached, no more terms for |z|

	KLPolRef pol = klPol(x,z);
	try{
	  klv[j].safeSubtract(pol,d,mu); // subtract q^d.mu.P_{x,z} from klv[j]
	}
	catch (error::NumericUnderflow& e){
	  throw kl_error::KLError(x,y,__LINE__,
				  static_cast<const KLContext&>(*this));
	}
      } // for {j)

  } // for (i)

}


/*!
  \brief Enters in |kl| the mu sum appearing a new K-L recursion.

  Precondition: s is real nonparity for y; |kl| already contains, for
  all $x$ that are primitive w.r.t.
  |y| in increasing order, the terms in $P_{x1,y}$ corresponding to
  $c_s.c_{y}$, where |x1| is $s.x$.
  The mu-table and KL-table have been filled in for elements of length < l(y).

  Explanation: the recursion formula involve a sum:
  $$
   P_{x,y}=  \sum_{x<z<y} mu(z,y) q^{(l(y)-l(z)-1)/2}P_{x,z}
  $$
  where |z| runs over the elements such that |s| is a descent for |z|.

  We construct a loop over |z|.  The test for
  $z<y$ is absent, but $\mu(z,y)\neq0$ implies $z\leq y$. The chosen
  loop order allows fetching
  $\mu(z,y)$ only once, and terminating the scan of |klv| once its values |x|
  become too large to produce a non-zero $P_{x,z}$.

  Can't use d_mu(y), which hasn't yet been written.
*/
    void Helper::muNewFormula(std::vector<KLPol>& klv, const PrimitiveRow& e,
			      size_t j, BlockElt y, size_t s)

{
  klv[j]=Zero;

  size_t l_y = length(y);
  // should iterate over z in the extremal row e for y, maybe
  // decreasing z, only odd length differences, stopping above length(x)

  BlockElt x = e[j];

    //following loop misses the z of length one less than y, since
    //they are not extremal. In our situation these are just inverse
    //Cayleys of y by various simple real roots.

    for (size_t k = e.size()- 2; length(e[k]) > length(x) ; --k)
      {
    BlockElt z = e[k];
    size_t l_z = length(z);

    if ((l_y - l_z +1) % 2) continue;

    DescentStatus::Value v = descentValue(s,z);
    if (not DescentStatus::isDescent(v))  continue;

    polynomials::Degree d = (l_y-l_z+1)/2; // power of q used in the
					   // loops below
    KLPolRef pol = klPol(x,z);

    KLPolRef mupol = klv[k];
    if (not(mupol.degree() == d-1)) continue; //now we have a mu coeff
    MuCoeff mu = mupol[d-1];
    if (mu==MuCoeff(1)) // avoid useless multiplication by 1 if possible

      {
	try {
	  klv[j].safeAdd(pol,d); // add q^d.P_{x,z} to klv[j]
	}
	catch (error::NumericOverflow& e){
	  throw kl_error::KLError(x,y,__LINE__,
				  static_cast<const KLContext&>(*this));
	}
      }
    else // mu!=MuCoeff(1)

	try{
	  klv[j].safeAdd(pol,d,mu); // add q^d.mu.P_{x,z} to klv[j]
	}
	catch (error::NumericOverflow& e){
	  throw kl_error::KLError(x,y,__LINE__,
				  static_cast<const KLContext&>(*this));
	}
  } // for (k)

    //now the missed z's one level down
    KLPol pol = Zero;
    for (size_t t = 0; t < rank(); ++t)
      {
    DescentStatus::Value v = descentValue(t,y);
    if( v==DescentStatus::RealTypeII or v==DescentStatus::RealTypeI ) {
      BlockElt z1 = inverseCayley(t,y).first;
    DescentStatus::Value w1 = descentValue(s,z1);
    if (DescentStatus::isDescent(w1))
      {
	pol.safeAdd(klPol(x,z1));
      }
    }
    if( v==DescentStatus::RealTypeI ) {
      BlockElt z2 = inverseCayley(t,y).second;
      DescentStatus::Value w2 = descentValue(s,z2);
      if (DescentStatus::isDescent(w2))
	{
	  pol.safeAdd(klPol(x,z2));
	}
    }
      }// simple real t

	try {
	  klv[j].safeAdd(pol,1); // add q.pol to klv[j]
	}
	catch (error::NumericOverflow& e){
	  throw kl_error::KLError(x,y,__LINE__,
				  static_cast<const KLContext&>(*this));
	}
} //muNewFormula

/*!
  \brief Fills in the row for y in the absence of complex descents for y.

  Precondition: every simple for y is a complex ascent, or imaginary, or real

  The real work is done by the newRecursionRow function.
*/
void Helper::newRecursion(BlockElt y)
{
  std::vector<KLPol> klv;
  PrimitiveRow e;

  // put result of recursion formula in klv
  makeExtremalRow(e,y); // list in e the x<=y with asc(x)\cup desc(y)=\emptyset
  newRecursionRow(klv,e,y);

  // write result
  writeRow(klv,e,y);

} //newRecursion


/*!
  \brief Puts in klv[i] the right-hand side of a recursion formula for
  P_{e[i],y} corresponding to some complex ascent s for x=e[i] that is real
  (necessarily nonparity) for y.

  Precondition: every simple root is for y either a complex ascent or
  imaginary or real (no complex descents for y).

  Explanation: the shape of the formula is roughly

    P_{x,y} = P_{x',y} +  correction term

  where x' is an ascent of x. The (c_s.c_{y1})-part depends on the
  status of x w.r.t. s (we look only at extremal x, so we know it is a
  descent). The correction term, coming from $\sum_z mu(z,y1)c_z$, is handled
  by |newMuCorrection|.
*/

// called only when y corresponds to theta-stable q with l split.
void Helper::newRecursionRow(std::vector<KLPol>& klv,
			     const PrimitiveRow& e,
			     BlockElt y)
{
  klv.resize(e.size());
  // last K-L polynomial is 1
  klv.back() = One;
  size_t j = klv.size()-1; ; // declare outside try block for error reporting
  try {
    while (j-->0) {
      BlockElt x = e[j];
      // we first seek an ascent for x whose ascent for y is real nonparity
      size_t s = firstNiceAscent(x,y);
      if (s < rank()) // there is such an ascent s
      {
	// start setting klv[j] to correction terms of new recursion formula
	muNewFormula(klv,e,j,y,s);

	if (descentValue(s,x)==DescentStatus::ComplexAscent)
	{
	  BlockElt x1 = cross(s,x);
	  klv[j].safeSubtract(lastklPol(klv,j+1,e,x1),1); // get P_{x1,y}
	  // subtract qP_{x1,y} from mu terms
	}//ComplexAscent case

	else if (descentValue(s,x)==DescentStatus::ImaginaryCompact)
	{
	  polynomials::Degree d = (length(y) - length(x) + 1)/2;
	  klv[j].safeQuotient(d-1); // add mu.q^d, divide by q+1
	}//ImaginaryCompact case

	else if (descentValue(s,x)==DescentStatus::ImaginaryTypeII)
	{
	  BlockEltPair x1 = cayley(s,x);
	  KLPol pol = lastklPol(klv,j+1,e,x1.first);
	  pol.safeAdd(lastklPol(klv,j+1,e,x1.second));
	    klv[j].safeAdd(pol);
	    klv[j].safeSubtract(pol,1); //now we've added (1-q)(P_{x1,y}
	    klv[j].safeDivide(2);   //this may throw
	} //ImaginaryTypeII case

	// no 'else'; we've handled all possible NiceAscents
      }//NiceAscent case

      else
      {
	/* just setting klv[j]=Zero; won't do here, even in C2. We need to use
	   idea on p. 8 of recursion.pdf. This means; find s1 and t adjacent
	   real for y, rn and r2, i1 and ic for x. Then successfully compute
	   P_{s1x,y} using t (direct recursion), and compute P_{sx,y}+P_{x,y}
	   using s. If no such s,t then P=0. */
	std::pair<size_t,size_t> st = firstImaginaryReal(x,y);
	size_t s1 = st.first;
	size_t t = st.second;

	if (s1 < rank())
	{
	  muNewFormula(klv,e,j,y,s1);
	  BlockElt xprime = cayley(s1,x).first;

	  klv[j].safeAdd(lastklPol(klv,j+1,e,xprime));

	  klv[j].safeSubtract(lastklPol(klv,j+1,e,xprime),1);//subtract
						   //(q-1)P_{xprime,y}

	  //now klv[j] holds P_{x,y}+P_{s1.x,y}
	  //compute P_{s1.x,y} using t

	  BlockElt s1x = cross(s1,x);
	  BlockElt x1 = cayley(t,s1x).first;

	  klv[j].safeSubtract(lastklPol(klv,j+1,e,x1));
	  if (descentValue(t,s1x)==DescentStatus::ImaginaryTypeII)
	  {
	    BlockElt x2 = cayley(t,s1x).second; // don't if t is i1 for x
	    klv[j].safeSubtract(lastklPol(klv,j+1,e,x2)); // don't if t is i1 for x
	  }
	}//firstImaginaryReal worked
	else
	  klv[j]=Zero;
      } //firstImaginaryReal case
  } // while (j-->0)
  }
  catch (error::NumericUnderflow& err) // repackage error, reporting x,y
  {
    throw kl_error::KLError(e[j],y,__LINE__,
			    static_cast<const KLContext&>(*this));
  }
}//newRecursionRow


/*!
  \brief Puts in klv the right-hand side of the recursion formula for y
  corresponding to the descent s.

  Precondition: s is either a complex, or a real type I or type II descent
  generator for y.

  Explanation: the shape of the formula is:

    P_{x,y} = (c_s.c_{y1})-part - correction term

  where y1 = cross(s,y) when s is complex for y, one of the two elements in
  inverseCayley(s,y) when s is real. The (c_s.c_{y1})-part depends on the
  status of x w.r.t. s (we look only at extremal x, so we know it is a
  descent). The correction term, coming from $\sum_z mu(z,y1)c_z$, is handled
  by |muCorrection|; the form of the summation depends only on |y1| (which it
  recomputes), but involves polynomials $P_{x,z}$ that depend on $x$ as well.
*/
void Helper::recursionRow(std::vector<KLPol>& klv,
			  const PrimitiveRow& e,
			  BlockElt y,
			  size_t s)
{
  BlockElt y1;

  if (descentValue(s,y) == DescentStatus::ComplexDescent) {
    y1 = cross(s,y);
  } else { // s is real type I or type II for y
    y1 = inverseCayley(s,y).first;
  }

  klv.resize(e.size());

  for (size_t j = 0; j < klv.size()-1; ++j) {
    BlockElt x = e[j];
    switch (descentValue(s,x)) {
    case DescentStatus::ImaginaryCompact: {
      // (q+1)P_{x,y1}
      klv[j] = klPol(x,y1);
      klv[j].safeAdd(klv[j],1);
    }
      break;
    case DescentStatus::ComplexDescent: {
      BlockElt x1 = cross(s,x);
      // P_{x1,y1}+q.P_{x,y1}
      klv[j] = klPol(x1,y1);
      klv[j].safeAdd(klPol(x,y1),1);
    }
      break;
    case DescentStatus::RealTypeI: {
      BlockEltPair x1 = inverseCayley(s,x);
      // P_{x1.first,y1}+P_{x1.second,y1}+(q-1)P_{x,y1}
      klv[j] = klPol(x1.first,y1);
      klv[j].safeAdd(klPol(x1.second,y1));
      klv[j].safeAdd(klPol(x,y1),1);
      try {
	klv[j].safeSubtract(klPol(x,y1));
      }
      catch (error::NumericUnderflow& e){
	throw kl_error::KLError(x,y,__LINE__,
				static_cast<const KLContext&>(*this));
      }
    }
      break;
    case DescentStatus::RealTypeII: {
      BlockElt x1 = inverseCayley(s,x).first;
      // P_{x_1,y_1}+qP_{x,y1}-P_{s.x,y1}

      klv[j] = klPol(x1,y1);

      klv[j].safeAdd(klPol(x,y1),1);
      try {
	klv[j].safeSubtract(klPol(cross(s,x),y1));
      }
      catch (error::NumericUnderflow& e){
	throw kl_error::KLError(x,y,__LINE__,
				static_cast<const KLContext&>(*this));
      }
    }
      break;
    default: // this cannot happen
      assert(false);
      break;
    }
  }

  // last K-L polynomial is 1
  klv.back() = One;

  // do mu-correction
  muCorrection(klv,e,y,s);

}//recursionRow


/*!
  \brief Writes down row y in d_kl and d_prim.

  Precondition: klv contains the polynomials corresponding to the
  extremal values in the row; er contains the corresponding (extremal
  with respect to y) block elements.

  Explanation: the difficulty is that we want to write down the _primitive_
  elements, i.e., those x for which all descents for y are either descents
  or imaginary type II ascents; and moreover, we write down only those values
  for which the polynomial is nonzero. The values for which there is an
  imaginary type II ascent are computed "on the fly", from higher up
  values in the row.
*/
void Helper::writeRow(const std::vector<KLPol>& klv,
		      const PrimitiveRow& er, BlockElt y)
{
  PrimitiveRow pr;
  makePrimitiveRow(pr,y);

  KLRow klr(pr.size()); // nonzero primitive entries (indexes into d_store)
  PrimitiveRow nzpr(pr.size()); // columns of nonzero prim. entries
  KLRow::iterator new_pol = klr.end();
  PrimitiveRow::iterator new_nzpr = nzpr.end();
  const PrimitiveRow::iterator nzpr_end = nzpr.end();

  // set stops in pr at extremal elements
  std::vector<size_t> stop(er.size()+1);
  stop[0] = 0;

  for (size_t j = 0; j < er.size(); ++j) {
    size_t epos = std::lower_bound(pr.begin(),pr.end(),er[j]) - pr.begin() + 1;
    stop[j+1] = epos;
  }

  // write polynomials, beginning with largest x (which is y).
  for (size_t j = er.size(); j;) {
    --j;
    // insert extremal polynomial
    if (not klv[j].isZero()) {
      *--new_nzpr = er[j];
      *--new_pol  = d_hashtable.match(klv[j]);
    }
    // do the others (primitive but not extremal ones), down to |stop[j]|
    for (size_t i = stop[j+1]-1; i > stop[j];) {
      --i;
      size_t s = firstAscent(descent(pr[i]),descent(y),rank());
      BlockEltPair x1 = cayley(s,pr[i]); // must be imaginary type II
      KLPol pol = klPol(x1.first,y,new_pol,new_nzpr,nzpr_end);
      pol.safeAdd(klPol(x1.second,y,new_pol,new_nzpr,nzpr_end));

      if (not pol.isZero()) {
	*--new_nzpr = pr[i];
	*--new_pol  = d_hashtable.match(pol);
      }
    }
  }

  // commit
  d_prim[y].reserve(klr.end() - new_pol);
  d_kl[y].reserve(klr.end() - new_pol);

  copy(new_nzpr,nzpr.end(),back_inserter(d_prim[y]));
  copy(new_pol,klr.end(),back_inserter(d_kl[y]));

  prim_size        += nzpr.end()-new_nzpr;
  nr_of_prim_nulls += new_nzpr  -nzpr.begin(); // measure unused space

}

} // namespace helper
} // namespace kl

/*****************************************************************************

        Chapter III -- The Thicket class

 *****************************************************************************/

namespace kl {
  namespace helper {


/*!
  \brief Constructs a thicket from y.

  Explanation: the thicket of y is the connected component of y in the graph
  whose edges are the real type II cross-actions. It is therefore contained
  in the R-packet of y. The edges of the thicket are a spanning tree; each
  edge goes from y1 to y2, where y2 is obtained from y1 through real cross
  action by s (the position of y2 in the vertex list, and s, are recorded
  in the edge). The recursion recorded is the recursion for s as a descent of
  y2. In this way, we can move along a path of edges and have the required
  recursion formulas available for computing the polynomial for y2 from
  that of y1 (all edges are two-sided, i.e., we also make the corresponding
  edge from y2 to y1.)
*/
Thicket::Thicket(Helper& h, BlockElt y)
  :base(h)
{
  typedef std::map<BlockElt,EdgeList> M;
  typedef M::iterator MI;

  std::stack<BlockElt> toDo; // used as a bag (a queue would do just as well)
  M thicket;

  toDo.push(y);
  thicket.insert(std::make_pair(y,EdgeList()));

  while (not toDo.empty()) {
    BlockElt y1 = toDo.top();
    toDo.pop();
    EdgeList& e1 = thicket.find(y1)->second; // y1 is known to exist in thicket
    for (size_t s = 0; s < rank(); ++s) {
      if (descentValue(s,y1) != DescentStatus::RealTypeII)
	continue;
      BlockElt y2 = cross(s,y1);
      std::pair<MI,bool> ins = thicket.insert(std::make_pair(y2,EdgeList()));
      if (ins.second) { // y2 was new (otherwise just ignore edge y1 -> y2)
	EdgeList& e2 = ins.first->second; // locate empty list just inserted
	e1.push_back(Edge(y1,y2,s,std::vector<KLPol>()));
	e2.push_back(Edge(y2,y1,s,std::vector<KLPol>()));
	toDo.push(y2);
      }
    }
  }

  // write out result

  // first flatten map to pair of vectors d_vertices, d_edges
  d_vertices.resize(thicket.size());
  d_edges.resize(thicket.size());
  size_t j = 0;

  for (MI i = thicket.begin(); i != thicket.end(); ++i) {
    d_vertices[j] = i->first;
    d_edges[j].swap(i->second);
    ++j;
  }

  // now modify d_egdes so that the source and y fields of its edges
  // are no longer interpreted as BlockElt, but as index into d_vertices
  for (size_t j = 0; j < d_edges.size(); ++j) {
    EdgeList& el = d_edges[j];
    for (size_t i = 0; i < el.size(); ++i) {
      // replace el[i].y and el[i].source by their positions in d_vertices
      el[i].source = std::lower_bound(d_vertices.begin(),d_vertices.end(),
				      el[i].source) - d_vertices.begin();
      el[i].y = std::lower_bound(d_vertices.begin(),d_vertices.end(),el[i].y)
	- d_vertices.begin();
    }
  }

  // make extremal lists
  d_extr.resize(thicket.size());

  for (size_t j = 0; j < d_extr.size(); ++j)
    base.makeExtremalRow(d_extr[j],d_vertices[j]);

  // make primitive lists
  d_prim.resize(thicket.size());

  for (size_t j = 0; j < d_prim.size(); ++j)
    base.makePrimitiveRow(d_prim[j],d_vertices[j]);

  // fill in recursions
  for (size_t j = 0; j < size(); ++j) {
    EdgeList& el = d_edges[j];
    for (size_t i = 0; i < el.size(); ++i) {
      BlockElt y2 = d_vertices[el[i].y];
      base.recursionRow(el[i].recursion,d_extr[el[i].y],y2,el[i].s);
    }
  }

  // resize kl lists
  d_klr.resize(size());
  d_firstKL.resize(size());
  d_firstPrim.resize(size());
}

/******** accessors **********************************************************/

/*!
  \brief Returns the position in d_vertices of the first y that is not
  extremal w.r.t. x, size() if there is no such y.
*/
size_t Thicket::nonExtremal(BlockElt x) const
{
  for (size_t j = 0; j < d_vertices.size(); ++j) {
    BlockElt y = d_vertices[j];
    size_t s = firstAscent(descent(x),descent(y),rank());
    if (s != rank()) // y was found
      return j;
  }

  return size();
}

/******** manipulators *******************************************************/


/*!
  \brief Checks if x has an ascent in row y_pos, and computes the K-L pol in
  that case.

  Precondition: x is primitive in the row;

  Explanation: If the ascent exists, it will be imaginary type II. if
  x1 = (x1.first,x1.second) is the corresponding Cayley transform, the
  formula is P_x = P_x1.first + P_x1.second, both of which can be read off
  from the known part of the row.
*/
bool Thicket::ascentCompute(BlockElt x, size_t pos)
{
  BlockElt y = d_vertices[pos];
  size_t s = firstAscent(descent(x),descent(y),rank());

  if (s == rank()) // no ascent
    return false;

  BlockEltPair x1 = base.cayley(s,x);
  KLPol pol = klPol(x1.first,pos);
  pol.safeAdd(klPol(x1.second,pos));

  if (not pol.isZero()) { // write pol
    *--d_firstKL[pos] = base.d_hashtable.match(pol);
    *--d_firstPrim[pos] = x;
  }

  return true;
}


/*!
  \brief Computes the K-L polynomial for x in row pos, using the recurrence
  relation from e.

  Precondition: x is extremal in the row; e points towards pos; the polynomial
  for x for the source of e is known.

  Explanation: P_{x,pos} + P_{x,source} will be given by the recurrence
  relation corresponding to x.
*/
void Thicket::edgeCompute(BlockElt x, size_t pos, const Edge& e)
{
  const PrimitiveRow& er = d_extr[pos];
  size_t xpos = std::lower_bound(er.begin(),er.end(),x) - er.begin();

  KLPol pol = e.recursion[xpos];

  try {
    pol.safeSubtract(klPol(x,e.source));
  }
  catch (error::NumericUnderflow& err){
    throw kl_error::KLError(x,d_vertices[pos],__LINE__,
			    static_cast<const KLContext&>(base));
  }

  if (not pol.isZero()) { // write pol
    *--d_firstKL[pos]   = base.d_hashtable.match(pol);
    *--d_firstPrim[pos] = x;
  }

}


/*!
  \brief Fills in the K-L polynomials for the elements of the thicket.

  Algorithm: for each element y_j of the thicket, we have the list of
  primitive elements pr[j], and a list of K-L polynomials klv[j]. We
  are going to fill in the polynomials downwards from the top (the top
  elements being all ones); at the end of the process, we will have
  iterators pointing into each pr[j] and each klv[j], where the
  extremal list from that iterator on will contain all x'es for which
  the klpol is non zero (in other words, the other ones have been
  "weeded out" from pr[j]), and klv[j] from the iterator on contains
  the corresponding polynomials. Then all that remains to do is copy
  these onto the corresponding lists of the context.  In order to
  achieve this, we put in d_xlist an ordered list of all elements that
  are primitive for _some_ y_j, not equal to y_j, and we move
  downwards in that list. It is guaranteed by Lemma 6.2 in David's
  notes, that for each x in d_xlist there is a y_j for which there is
  an easy reduction (i.e., x might be primitive for y_j, but not
  extremal.) So we can deduce P_{x,y_j} from the already known part of
  klv[j], and then traversing the tree structure of edge relations
  imposed on the thicket, we can compute all the other P_{x,y_i} that
  need to be (i.e., those for which x is in pr[j].) We record the
  result only if the corresponding polynomial is non-zero.
*/
void Thicket::fill()
{
  fillXList();

  // initialize rows, fill in last element, initialize iterators
  for (size_t j = 0; j < size(); ++j) {
    const PrimitiveRow& pr = primitiveRow(j); // last element is y_j
    d_klr[j].resize(pr.size());
    d_klr[j].back() = base.d_one;
    d_firstPrim[j] = d_prim[j].end()-1;
    d_firstKL[j] = d_klr[j].end()-1;
  }

  // fill in rows
  for (size_t j = d_xlist.size(); j>0;) {
    --j;
    BlockElt x = d_xlist[j];
    // find a vertex s.t. x is not extremal
    size_t iy = nonExtremal(x);
    if (iy == size()) // do nothing; zero polynomials are ignored
      continue;
    for (ThicketIterator i(*this,iy); i(); ++i) {
      size_t pos = *i;
      const PrimitiveRow& pr = primitiveRow(pos);
      if (not std::binary_search(pr.begin(),pr.end(),x))
	// x is not primitive
	continue;
      if (ascentCompute(x,pos)) // x is not extremal
	continue;
      // if we get here, x is extremal
      edgeCompute(x,pos,i.edge());
    }
  }

  // write result

  for (size_t j = 0; j < size(); ++j) {
    BlockElt y = d_vertices[j];

    base.d_prim[y].reserve(d_prim[j].end() - d_firstPrim[j]);
    base.d_kl[y].reserve(d_prim[j].end() - d_firstPrim[j]);

    copy(d_firstPrim[j],d_prim[j].end(),back_inserter(base.d_prim[y]));
    copy(d_firstKL[j],d_klr[j].end(),back_inserter(base.d_kl[y]));

    base.prim_size        += d_prim[j].end()-d_firstPrim[j];
    base.nr_of_prim_nulls += d_firstPrim[j]-d_prim[j].begin();
  }

}


/*!
  \brief Puts in d_xlist the union of the extremal lists for the elements
  of the thicket, top element excluded.
*/
void Thicket::fillXList()
{
  std::set<BlockElt> xs;

  for (size_t j = 0; j < size(); ++j)
  {
    const PrimitiveRow& p = primitiveRow(j);
    for (size_t i = 0; i < p.size()-1; ++i)
      xs.insert(p[i]);
  }

  d_xlist.reserve(xs.size());
  copy(xs.begin(),xs.end(),std::back_inserter(d_xlist));

}

  }
}

/*****************************************************************************

        Chapter IV -- The ThicketIterator class implementation

 *****************************************************************************/

namespace kl {
  namespace helper {

ThicketIterator::ThicketIterator(const Thicket& th, size_t j)
  : d_thicket(&th)

{
  d_stack.push(j);
  d_done.set_capacity(th.size()); // all bits unset initially
  d_done.insert(j);
  d_current = th.edgeList(j).begin();
  d_currentEnd = th.edgeList(j).end();
  d_pos = j;
}

/******** manipulators *******************************************************/

/*!
  \brief Pre-increment operator.

  Incrementing the iterator means going to the next new element, obtained as
  destination vertex of one of the remaining edges in the current edgelist, or
  in an edgelist for a vertex popped from the stack if necessary; here 'new'
  meaning the vertex has not yet been recorded in d_done. If no new
  destination vertex can be obtained even after trying all lists on the stack
  we return the thicket size as indication that no vertices are left. In the
  contrary case the vertex returned is recorded in d_done and pushed onto the
  stack for later processing of its neighbors.
*/
ThicketIterator& ThicketIterator::operator++ ()
{
  for (; d_current != d_currentEnd; ++d_current) {
    // get index of destination of current edge in vertices
    size_t j = d_current->y;
    if (not d_done.isMember(j)) { // j is new
      d_pos = j;
      d_stack.push(j);
      d_done.insert(j);
      return *this;
    }
  }

  // if we get here, we have exhausted the current row

  while (not d_stack.empty()) {
    size_t j = d_stack.top();
    d_stack.pop();

    // make edge list of element just popped the current one
    d_current = d_thicket->edgeList(j).begin();
    d_currentEnd = d_thicket->edgeList(j).end();

    // and do exactly what was done
    for (; d_current != d_currentEnd; ++d_current) {
      size_t i = d_current->y;
      if (not d_done.isMember(i)) { // i is new
	d_pos = i;
	d_stack.push(i);
	d_done.insert(i);
	return *this;
      }
    }
  }

  // if we get here, we have exhausted the thicket
  d_pos = d_thicket->size();

  return *this;
}

} //namespace helper
} //namespace kl


/*****************************************************************************

        Chapter V -- Functions declared in kl.h

 *****************************************************************************/

namespace kl {


/*!
  \brief Puts in wg the W-graph for this block.

  Explanation: the W-graph is a graph with one vertex for each element of the
  block; the corresponding descent set is the tau-invariant, i.e. the set of
  generators s that are either complex descents, real type I or II, or
  imaginary compact. Let x < y in the block such that mu(x,y) != 0, and
  descent(x) != descent(y). Then there is an edge from x to y unless
  descent(x) is contained in descent(y), and an edge from y to x unless
  descent(y) is contained in descent(x). Note that the latter containment
  always holds when the length difference is > 1, so that in that case there
  will only be an edge from x to y (the edge must be there because we already
  assumed that the descent sets were not equal.) In both cases, the
  coefficient corresponding to the edge is mu(x,y).

  NOTE: if I'm not mistaken, the edgelists come already out sorted.
*/
void wGraph(wgraph::WGraph& wg, const KLContext& klc)
{
  wg.reset();
  wg.resize(klc.size());

  // fill in descent sets
  for (BlockElt y = 0; y < klc.size(); ++y)
    wg.descent(y) = klc.descentSet(y);

  // fill in edges and coefficients
  for (BlockElt y = 0; y < klc.size(); ++y) {
    const RankFlags& d_y = wg.descent(y);
    const MuRow& mrow = klc.muRow(y);
    for (size_t j = 0; j < mrow.first.size(); ++j) {
      BlockElt x = mrow.first[j];
      const RankFlags& d_x = wg.descent(x);
      if (d_x == d_y)
	continue;
      MuCoeff mu = mrow.second[j];
      if (klc.length(y) - klc.length(x) > 1) { // add edge from x to y
	wg.edgeList(x).push_back(y);
	wg.coeffList(x).push_back(mu);
	continue;
      }
      // if we get here, the length difference is 1
      if (not d_y.contains(d_x)) { // then add edge from x to y
	wg.edgeList(x).push_back(y);
	wg.coeffList(x).push_back(mu);
      }
      if (not d_x.contains(d_y)) { // then add edge from y to x
	wg.edgeList(y).push_back(x);
	wg.coeffList(y).push_back(mu);
      }
    }
  }

}

} // namespace kl

/*****************************************************************************

        Chapter VI -- Functions local to this module

 *****************************************************************************/

namespace kl {
  namespace helper {


/*!
  \brief Returns the first s that is an ascent for d1, and a descent for d2;
  rank if there is none such.

  Precondition: rank is the number of valid fields in d1 and d2.
*/
size_t firstAscent(const DescentStatus& d1,
		   const DescentStatus& d2, size_t rank)
{
  for (size_t s = 0; s < rank; ++s)
    if ((DescentStatus::isDescent(d2[s])) and
	(not DescentStatus::isDescent(d1[s])))
      return s;

  return rank;
}


/*!
  \brief Returns the first s that is a non-ImaginaryTypeII ascent for
  d1, and a descent for d2; rank if there is none such.

  Precondition: rank is the number of valid fields in d1 and d2.
*/
size_t goodAscent(const DescentStatus& d1,
		  const DescentStatus& d2, size_t rank)
{
  for (size_t s = 0; s < rank; ++s) {
    if ((not DescentStatus::isDescent(d2[s])) or
	(DescentStatus::isDescent(d1[s])))
      continue;
    if (d1[s] != DescentStatus::ImaginaryTypeII)
      return s;
  }

  return rank;
}

} // namespace helper
} // namespace kl
} // namespace atlas
