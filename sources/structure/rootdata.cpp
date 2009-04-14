/*!
\file
  \brief Implementation of the RootDatum class.

  What we call a root datum in this program is what is usually called
  a based root datum.

  The root datum defines the complex reductive group entirely.

  Another non-trivial issue is how to get a group interactively from the
  user. Actually this is perhaps a bit overstated here; in fact, when the
  program functions as a library, the interaction with the user will be
  relegated to some higher-up interface; in practice it is likely that
  the data will usually be read from a file. The main issue is to define
  the character lattice of the torus.

  In the interactive approach, we start from the abstract real lie
  type, and (implicitly) associate to it the direct product of the
  simply connected semisimple factor, and the torus as it is
  given. The user is presented with the torsion subgroup Z_tor of the
  center, written as a factor of Q/Z for each torus factor, and a
  finite abelian group for each simple factor.  The user must then
  choose generators of a finite subgroup of Z_tor; this subgroup
  corresponds to a sublattice of finite index in the character
  lattice, containing the root lattice. From this sublattice the
  actual root datum is constructed.
*/
/*
  This is rootdata.cpp.

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Reductive Lie Groups

  For license information see the LICENSE file
*/

#include "rootdata.h"

#include <cassert>
#include <set>

#include "arithmetic.h"
#include "dynkin.h"
#include "lattice.h"
#include "partition.h"
#include "prerootdata.h"
#include "smithnormal.h"

/*****************************************************************************

  This module contains the implementation of the RootDatum class. What we
  call a root datum in this program is what is usually called a based root
  datum.

  The root datum defines the complex reductive group entirely.

  Another non-trivial issue is how to get a group interactively from the
  user. Actually this is perhaps a bit overstated here; in fact, when the
  program functions as a library, the interaction with the user will be
  relegated to some higher-up interface; in practice it is likely that
  the data will usually be read from a file. The main issue is to define
  the character lattice of the torus.

  In the interactive approach, we start from the abstract real lie
  type, and (implicitly) associate to it the direct product of the
  simply connected semisimple factor, and the torus as it is
  given. The user is presented with the torsion subgroup Z_tor of the
  center, written as a factor of Q/Z for each torus factor, and a
  finite abelian group for each simple factor.  The user must then
  choose generators of a finite subgroup of Z_tor; this subgroup
  corresponds to a sublattice of finite index in the character
  lattice, containing the root lattice. From this sublattice the
  actual root datum is constructed.

  [DV: An earlier version of this class included the inner class of
  real groups, which is to say an involutive automorphism of the based
  root datum.  As an artifact of that, the user interaction acquires
  the involutive automorphism before the subgroup of Z_tor, and
  insists that the subgroup of Z_tor be preserved by this involution.]

******************************************************************************/

namespace atlas {


/*****************************************************************************

        Chapter I -- The RootDatum class

  The root datum class will be one of the central classes in the program.

  A based root datum is a quadruple (X,X^,D,D^), where X and X^ are lattices
  dual to each other, and D is a basis for a root system in X, D^ the dual
  basis of the dual root system (more precisely, D and D^ willl be bases
  for root systems in the sub-lattices they generate.)

  The fundamental lattice X~ that we will work in is the character lattice of
  a group G~ x T, where G~ is a simply connected semisimple group, and T
  a torus; our complex reductive group G will be a finite
  quotient of this. More precisely, let Q be the subgroup of our lattice
  generated by D; then the character lattice X of G may be any sublattice of
  full rank of our fundamental lattice containing Q.

******************************************************************************/

namespace rootdata {

struct RootSystem::root_compare
{
  root_compare() {}
  bool operator()(const Byte_vector& alpha, const Byte_vector& beta)
  {
    int d;
    for (size_t i=alpha.size(); i-->0;)
      if ((d=alpha[i]-beta[i])!=0 )
	return d<0;
    return false; // equal, therefore not less
  }
};

RootSystem::RootSystem(const latticetypes::LatticeMatrix& Cartan_matrix)
  : rk(Cartan_matrix.numRows())
  , Cmat(rk*rk) // filled below
  , ri()
  , root_perm()
{
  if (rk>0)
    cons(Cartan_matrix);
}

void RootSystem::cons(const latticetypes::LatticeMatrix& Cartan_matrix)
{
  std::vector<Byte_vector> simple_root(rk,Byte_vector(rk));
  std::vector<Byte_vector> simple_coroot(rk,Byte_vector(rk));
  std::vector<RootList> link;
  std::vector<std::set<Byte_vector,root_compare> >
    roots_of_length(4*rk); // more than enough if |rk>0|; $E_8$ needs size 31

  for (size_t i=0; i<rk; ++i)
  {
    Byte_vector e_i(rk,0);
    e_i[i]=1; // set to standard basis for simple roots
    roots_of_length[1].insert(e_i);
    for (size_t j=0; j<rk; ++j)
      Cartan_entry(i,j) = simple_root[i][j] = simple_coroot[j][i] =
	Cartan_matrix(i,j);
  }

  // construct positive root list, simple reflection links, and descent sets

  RootList first_l(1,0);
  for (size_t l=1; not roots_of_length[l].empty(); ++l)// empty level means end
  {
    first_l.push_back(ri.size()); // set |first_l[l]| to next root to be added
    for (std::set<Byte_vector>::iterator
	   it=roots_of_length[l].begin(); it!=roots_of_length[l].end(); ++it)
    {
      const Byte_vector& alpha = *it;

      const RootNbr cur = ri.size();
      ri.push_back(root_info(alpha)); // add new positive root to the list
      link.push_back(RootList(rk,~0u)); // initially all links are undefined

      byte c;
      for (size_t i=0; i<rk; ++i)
	if ((c=alpha.dot(simple_coroot[i]))==0) // orthogonal
	  link[cur][i]=cur; // point to root itself
	else
	{
	  Byte_vector beta=alpha; // make a copy
	  beta[i]-=c; // increase coefficient; add |-c| times |alpha[i]|
	  if (c>0) // positive scalar product means $i$ gives a \emph{descent}
	  {
	    ri[cur].descents.set(i);
	    if (l==1)
	    {
	      assert(i==cur); // a simple root is its own unique descent
	      link[cur][i]=cur; // but reflection is actually minus itself
	    }
	    else
	      for (RootNbr j=first_l[l-c]; j<first_l[l-c+1]; ++j) // look down
		if (root(j)==beta)
		{
		  link[cur][i]=j; // link current root downward to root |j|
		  link[j][i]=cur; // and install reciprocal link at |j|
		  break;
		}
	    assert(link[cur][i]!=~0u); // some value must have been set
	  }
	  else // |c<0| so, reflection adding |-c| times $\alpha_j$, goes up
	  {
	    ri[cur].ascents.set(i);
	    roots_of_length[l-c].insert(beta); // create root at proper length
	  }
	}
    }
    roots_of_length[l].clear(); // no longer needed
  }

  size_t npos = ri.size(); // number of positive roots

  // simple coroots in themselves are just standard basis, like simple roots
  for (size_t i=0; i<rk; ++i)
    coroot(i) = root(i);

  // complete with computation of non-simple positive coroots
  for (RootNbr alpha=rk; alpha<npos; ++alpha)
  {
    size_t i = ri[alpha].descents.firstBit();
    RootNbr beta = link[alpha][i];
    assert(beta<alpha); // so |coroot(beta)| is already defined
    coroot(alpha)=coroot(beta); // take a copy
    coroot(alpha)[i]-=coroot(beta).scalarProduct(simple_root[i]); // and modify
  }

  root_perm.resize(npos,setutils::Permutation(2*npos));
  // first fill in the simple root permutations
  for (size_t i=0; i<rk; ++i)
  {
    setutils::Permutation& perm=root_perm[i];
    for (RootNbr alpha=0; alpha<npos; ++alpha)
      if (alpha==i) // simple root reflecting itself makes it negative
      {
	perm[npos+alpha]=npos-1-alpha;
	perm[npos-1-alpha]=npos+alpha;
      }
      else // don't change positivity status
      {
	RootNbr beta = link[alpha][i];
	perm[npos+alpha] = npos+beta;
	perm[npos-1-alpha]=npos-1-beta;
      }
  }

  // extend permutations to all positive roots by conjugation from lower root
  for (size_t alpha=rk; alpha<npos; ++alpha)
  {
    size_t i=ri[alpha].descents.firstBit();
    assert(i<rk);
    setutils::Permutation& alpha_perm=root_perm[alpha];
    alpha_perm=root_perm[i];
    root_perm[link[alpha][i]].left_mult(alpha_perm);
    root_perm[i].left_mult(alpha_perm);
  }

}

RootSystem::RootSystem(const RootSystem& rs, tags::DualTag)
  : rk(rs.rk)
  , Cmat(rs.Cmat) // transposed below
  , ri(rs.ri)     // entries modified internally in non simply laced case
  , root_perm(rs.root_perm) // unchanged
{
  bool simply_laced = true;
  for (size_t i=0; i<rk; ++i)
    for (size_t j=i+1; j<rk; ++j) // do only case $i<j$, upper triangle
      if (Cartan_entry(i,j)!=Cartan_entry(j,i))
	simply_laced = false , std::swap(Cartan_entry(i,j),Cartan_entry(j,i));

  if (not simply_laced)
    for (RootNbr alpha=0; alpha<numPosRoots(); ++alpha)
      root(alpha).swap(coroot(alpha)); // |descent|, |ascent| are OK
}

latticetypes::LatticeElt RootSystem::root_expr(RootNbr alpha) const
{
  RootNbr a=abs(alpha);
  latticetypes::LatticeElt expr(root(a).begin(),root(a).end());
  if (not isPosRoot(alpha))
    expr *= -1;
  return expr;
}

latticetypes::LatticeElt RootSystem::coroot_expr(RootNbr alpha) const
{
  RootNbr a=abs(alpha);
  latticetypes::LatticeElt expr(ri[a].dual.begin(),ri[a].dual.end());
  if (not isPosRoot(alpha))
    expr *= -1;
  return expr;
}

latticetypes::LatticeMatrix RootSystem::cartanMatrix() const
{
  latticetypes::LatticeMatrix result(rk,rk);

  for (size_t i=0; i<rk; ++i)
    for (size_t j=0; j<rk; ++j)
      result(i,j) = Cartan_entry(i,j);

  return result;
}

/*!
\brief Returns the Cartan matrix of the root subsystem with basis |rb|.
*/
latticetypes::LatticeMatrix RootSystem::cartanMatrix(const RootList& rb) const
{
  size_t r = rb.size();

  latticetypes::LatticeMatrix result(r,r);

  for (size_t i=0; i<r; ++i)
    for (size_t j=0; j<r; ++j)
      result(i,j) = bracket(rb[i],rb[j]);

  return result;
}

latticetypes::LatticeCoeff
RootSystem::bracket(RootNbr alpha, RootNbr beta) const // $\<\alpha,\beta^\vee>$
{
  if (isOrthogonal(alpha,beta)) // this is quick
    return 0;

  const Byte_vector& row = root(abs(alpha));
  const Byte_vector& col = coroot(abs(beta));

  latticetypes::LatticeCoeff c=0;
  for (size_t i=0; i<rk; ++i)
    for (size_t j=0; j<rk; ++j)
      c += row[i]*Cartan_entry(i,j)*col[j];

  return isPosRoot(alpha)^isPosRoot(beta) ? -c : c;
}

setutils::Permutation
RootSystem::extend_to_roots(const RootList& simple_image) const
{
  assert(simple_image.size()==rk);
  setutils::Permutation result(numRoots());

  RootList image_reflection(rk);

  // prepare roots for reflections for roots in |simple_image|
  for (size_t i=0; i<rk; ++i)
    image_reflection[i] = abs(simple_image[i]);

  // copy images of simple roots
  for (size_t i=0; i<rk; ++i)
    result[simpleRootNbr(i)]=simple_image[i];

  // extend to positive roots
  for (size_t alpha=numPosRoots()+rk; alpha<numRoots(); ++alpha)
  {
    size_t i = ri[alpha-numPosRoots()].descents.firstBit();
    assert(i<rk);
    RootNbr beta = simple_reflected_root(alpha,i);
    assert(isPosRoot(beta) and beta<alpha);
    result[alpha] = root_perm[image_reflection[i]][result[beta]];
  }

  // finally extend to negative roots, using symmetry of root permutation
  for (size_t alpha=0; alpha<numPosRoots(); ++alpha) // |alpha| negative root
    result[alpha]=rootMinus(result[rootMinus(alpha)]);

  return result;
}

setutils::Permutation
RootSystem::root_permutation(const setutils::Permutation& twist) const
{
  assert(twist.size()==rk);
  RootList simple_image(rk);
  for (size_t i=0; i<twist.size(); ++i)
    simple_image[i] = simpleRootNbr(twist[i]);

  return extend_to_roots(simple_image);
}

weyl::WeylWord RootSystem::reflectionWord(RootNbr r) const
{
  RootNbr alpha = isPosRoot(r) ? r : rootMinus(r); // make |alpha| positive

  weyl::WeylWord result;

  while (alpha>=rk+numPosRoots()) // alpha positive but not simple
  {
    size_t i = ri[alpha-numPosRoots()].descents.firstBit();
    result.push_back(i);
    alpha = root_perm[i][alpha];
  }
  result.reserve(2*result.size()+1);
  result.push_back(alpha-numPosRoots()); // central reflection
  for (size_t i=result.size()-1; i-->0;) // trace back to do conjugation
    result.push_back(result[i]);

  return result;
}


RootList RootSystem::simpleBasis(RootSet rs) const
{
  rs.clear(0,numPosRoots()); // clear negative roots
  RootSet candidates = rs; // candidates for being simple

  for (RootSet::iterator it=candidates.begin(); it(); ++it)
  {
    RootNbr alpha=*it;
    for (RootSet::iterator jt=rs.begin(); jt(); ++jt)
    {
      RootNbr beta=*jt;
      if (alpha==beta) continue; // avoid reflecting root itself
      RootNbr gamma = root_perm[alpha-numPosRoots()][beta];
      if (gamma<beta) // positive dot product
      {
	if (isPosRoot(gamma))
	  candidates.remove(beta); // simple root cannot be made less positive
	else
	{
	  candidates.remove(alpha); // simple root cannot make other negative
	  break;
	}
      }
    }
    if (not candidates.isMember(alpha)) // so we just removed it, |break| again
      break;
  }
  // now every member of |candidates| permutes the other members of |rs|

  return RootList(candidates.begin(), candidates.end()); // converts to vector
}

bool RootSystem::sumIsRoot(RootNbr alpha, RootNbr beta, RootNbr& gamma) const
{
  RootNbr a = abs(alpha);
  RootNbr b = abs(beta);

  bool alpha_less = root_compare()(root(a),root(b));
  if (alpha_less) // compare actual levels
    std::swap(a,b); // ensure |a| is higher root

  Byte_vector v =
    isPosRoot(alpha)^isPosRoot(beta)
    ? alpha_less ? root(b) - root(a) : root(a) - root(b)
    : root(a) + root(b);

  for (RootNbr i=0; i<ri.size(); ++i) // no ordering can be assumed
    if (v==root(i))
    {
      gamma = isPosRoot(alpha_less ? beta : alpha)
	? i+numPosRoots() : numPosRoots()-1-i;
      return true;
    }

  return false; // not found
}

/*! \brief
  Makes the orthogonal system |rset| into an equaivalent (for |refl_prod|) one
  that is additively closed inside the full root system.

  This can be achieved by repeatedly replacing a pair of short roots spanning
  a B2 subsystem by a pair of long roots of that subsystem. Although we record
  no information about relative root lengths it is easily seen that the long
  roots in some B2 subsystem can never be short roots in another subsystem, so
  there is no need to assure newly created roots are inspected subsequently.
*/
RootSet RootSystem::long_orthogonalize(const RootSet& rset) const
{
  RootSet result = rset;
  RootNbr gamma;
  for (RootSet::iterator it=result.begin(); it(); ++it)
    for (RootSet::iterator jt=result.begin(); jt!=it; ++jt)
      if (sumIsRoot(*it,*jt,gamma))
      { // replace *it and *jt by sum and difference (short->long in B2 system)
	result.insert(gamma);
	result.insert(root_permutation(*jt)[gamma]);
	result.remove(*it); result.remove(*jt);
	break; // move to next |*it|; without this inner loop won't terminate!
      }
  return result;
}


RootList RootSystem::high_roots() const
{
  RootList h;
  for (RootNbr alpha=0; alpha<numPosRoots(); ++alpha) // traverse negative roots
    if (descent_set(alpha).none())
      h.push_back(rootMinus(alpha));

  return h;
}



RootDatum::RootDatum(const prerootdata::PreRootDatum& prd)
  : RootSystem(prd.Cartan_matrix())
  , d_rank(prd.rank())
  , d_roots(numRoots())   // dimension only
  , d_coroots(numRoots()) // same for coroots
  , weight_numer(), coweight_numer()
  , d_radicalBasis(), d_coradicalBasis()
  , d_2rho(d_rank,0)
  , d_dual_2rho(d_rank,0)
  , Cartan_denom()
  , d_status()
{
  latticetypes::LatticeMatrix root_mat(prd.roots());
  latticetypes::LatticeMatrix coroot_mat(prd.coroots());
  for (RootNbr alpha=numPosRoots(); alpha<numRoots(); ++alpha)
  {
    d_roots[alpha]= root_mat.apply(root_expr(alpha));
    d_coroots[alpha] = coroot_mat.apply(coroot_expr(alpha));
    d_2rho += d_roots[alpha];
    d_dual_2rho += d_coroots[alpha];
    d_roots[rootMinus(alpha)] = -d_roots[alpha];
    d_coroots[rootMinus(alpha)] = -d_coroots[alpha];
  }


  // the simple weights are given by the matrix Q.tC^{-1}, where Q is the
  // matrix of the simple roots, tC the transpose Cartan matrix
  latticetypes::LatticeMatrix iC = cartanMatrix().inverse(Cartan_denom);
  weight_numer = (root_mat*iC.transposed()).columns();

  // for the simple simple coweights, use coroots and (plain) Cartan matrix
  coweight_numer = (coroot_mat*iC).columns();

  // get basis of co-radical character lattice, if any (or leave empty list)
  if (semisimpleRank()<d_rank)
  {
    lattice::perp(d_coradicalBasis,d_coroots,d_rank);
    lattice::perp(d_radicalBasis,d_roots,d_rank);
  }

  // fill in the status
  fillStatus();
}



/*!
\brief Constructs the root system dual to the given one.

  Since we do not use distinct types for weights and coweights, we can proceed
  by interchanging roots and coroots. The ordering of the roots corresponds to
  that of the original root datum (root |i| of the dual datum is coroot |i| of
  the orginal datum; this is not the ordering that would have been used in a
  freshly constructed root datum), but users should \emph{not} depend on this.
*/
RootDatum::RootDatum(const RootDatum& rd, tags::DualTag)
  : RootSystem(rd,tags::DualTag())
  , d_rank(rd.d_rank)
  , d_roots(rd.d_coroots)
  , d_coroots(rd.d_roots)
  , weight_numer(rd.coweight_numer)
  , coweight_numer(rd.weight_numer)
  , d_radicalBasis(rd.d_coradicalBasis)
  , d_coradicalBasis(rd.d_radicalBasis)
  , d_2rho(rd.d_dual_2rho)
  , d_dual_2rho(rd.d_2rho)
  , Cartan_denom(rd.Cartan_denom)
  , d_status()
{
  // fill in the status

  fillStatus();

  assert(d_status[IsAdjoint] == rd.d_status[IsSimplyConnected]);
  assert(d_status[IsSimplyConnected] == rd.d_status[IsAdjoint]);
}

RootDatum::~RootDatum()

{}

/******** accessors **********************************************************/

/*!
\brief Returns the permutation of the roots induced by |q|.

  Precondition: |q| permutes the roots;
*/
setutils::Permutation
  RootDatum::rootPermutation(const LT::LatticeMatrix& q) const
{
  setutils::Permutation result(numRoots());

  for (RootNbr alpha=0; alpha<numRoots(); ++alpha)
  {
    result[alpha] = rootNbr(q.apply(root(alpha)));
    assert(result[alpha]<numRoots()); // image by |q| must be some root
  }

  return result;
}



/*!
\brief Returns the reflection for root \#alpha.

  NOTE: this is not intended for heavy use. If that is envisioned, it would be
  better to construct the matrices once and for all and return const
  references.
*/
latticetypes::LatticeMatrix RootDatum::root_reflection(RootNbr alpha) const
{
  latticetypes:: LatticeMatrix result;
  matrix::identityMatrix(result,d_rank);

/*
  result -=
    matrix::column_matrix(root(alpha)) * matrix::row_matrix(coroot(alpha));
*/
  const Root& root = d_roots[alpha];
  const Root& coroot = d_coroots[alpha];

  for (size_t i=0; i<d_rank; ++i)
    for (size_t j=0; j<d_rank; ++j)
      result(i,j) -= root[i]*coroot[j];

  return result;
}

weyl::WeylWord RootDatum::reflectionWord(RootNbr alpha) const
{
  return toPositive(reflection(twoRho(),alpha),*this);
}



/*!\brief
  Returns the expression of $q^{-1}$ as a product of simple reflections.

  Precondition: $q$ gives the action on the weight lattice of some Weyl group
  element

  Algorithm: apply $q$ to |twoRho|, then use |toPositive| to find a Weyl
  word making it dominant again, which by assumption expresses $q^{-1}$.
*/
weyl::WeylWord RootDatum::word_of_inverse_matrix
  (const latticetypes::LatticeMatrix& q) const
{
  return toPositive(q.apply(twoRho()),*this);
}

/*!
\brief Returns the sum of the positive roots in rl.

  Precondition: rl holds the roots in a sub-rootsystem of the root system of
  rd;
*/
latticetypes::Weight RootDatum::twoRho(const RootList& rl) const
{
  latticetypes::Weight result(rank(),0);

  for (size_t i = 0; i < rl.size(); ++i)
    if (isPosRoot(rl[i]))
      result += root(rl[i]);

  return result;
}

/*!
\brief Returns the sum of the positive roots in rs.

  Precondition: rs holds the roots in a sub-rootsystem of the root system of
  rd;
*/
latticetypes::Weight RootDatum::twoRho(const RootSet& rs) const
{
  latticetypes::Weight result(rank(),0);

  for (RootSet::iterator i = rs.begin(); i(); ++i)
    if (isPosRoot(*i))
      result += root(*i);

  return result;
}



/******** manipulators *******************************************************/

void RootDatum::swap(RootDatum& other)
{
  std::swap(d_rank,other.d_rank);
  d_roots.swap(other.d_roots);
  d_coroots.swap(other.d_coroots);
  weight_numer.swap(other.weight_numer);
  coweight_numer.swap(other.coweight_numer);
  d_radicalBasis.swap(other.d_radicalBasis);
  d_coradicalBasis.swap(other.d_coradicalBasis);
  d_2rho.swap(other.d_2rho);
  d_dual_2rho.swap(other.d_dual_2rho);
  std::swap(Cartan_denom,other.Cartan_denom);
  d_status.swap(other.d_status);
}

/******** private member functions ******************************************/


/*!
\brief Fills in the status of the rootdatum.
*/
void RootDatum::fillStatus()
{
  latticetypes::LatticeMatrix
    q(beginSimpleRoot(),endSimpleRoot(),tags::IteratorTag());

  latticetypes::WeightList b;
  latticetypes::CoeffList invf;

  matrix::initBasis(b,d_rank);
  smithnormal::smithNormal(invf,b.begin(),q);

  d_status.set(IsAdjoint);

  for (size_t i = 0; i < invf.size(); ++i)
    if (invf[i] != 1)
    {
      d_status.reset(IsAdjoint);
      break;
    }

  latticetypes::LatticeMatrix
    qd(beginSimpleCoroot(),endSimpleCoroot(),tags::IteratorTag());

  matrix::initBasis(b,d_rank);
  invf.clear();
  smithnormal::smithNormal(invf,b.begin(),qd);

  d_status.set(IsSimplyConnected);

  for (size_t i = 0; i < invf.size(); ++i)
    if (invf[i] != 1)
    {
      d_status.reset(IsSimplyConnected);
      break;
    }
}

} // namespace rootdata

/*****************************************************************************

        Chapter II -- Functions declared in rootdata.h

******************************************************************************/

namespace rootdata {


/*!
\brief Returns matrix of dual involution of the one given by |i|

  Precondition: |q| is an involution of |rd| as a _based_ root datum

  Postcondition: |di| is an involution of the dual based root datum

  Formula: $(w_0^t)(-q^t) = (-q.w_0)^t$

  In other words, to |-i| we apply the (longest) Weyl group element $w_0$
  making the image of the dominant chamber dominant again, then transpose

  Since $-w_0$ is central in the group of based root datum automorphisms, it
  doesn't matter whether one multiplies by $w_0$ on the left or on the right
*/
LT::LatticeMatrix dualBasedInvolution
  (const LT::LatticeMatrix& q, const RootDatum& rd)
{
  weyl::WeylWord ww = toPositive(-rd.twoRho(),rd);
  return (q*rd.matrix(ww)).negative_transposed();
}


/*!
\brief Returns the elements of |subsys| which are orthogonal to all
  elements of |o|.
*/
RootSet makeOrthogonal(const RootSet& o, const RootSet& subsys,
		       const RootSystem& rs)
{
  RootSet candidates = subsys;
  candidates.andnot(o); // roots of |o| itself need not be considered

  RootSet result = candidates;

  for (RootSet::iterator it = candidates.begin(); it(); ++it)
  {
    RootNbr alpha = *it;
    RootSet::iterator jt;
    for (jt = o.begin(); jt(); ++jt)
      if (not rs.isOrthogonal(alpha,*jt))
      {
	result.remove(alpha);
	break;
      }
  }

  return result;
}


/*!
\brief Transforms q into w.q, where w is the unique element such that
  w.q fixes the positive Weyl chamber.

  Note that wq is then automatically an involution, permuting the simple roots
*/

void toDistinguished(latticetypes::LatticeMatrix& q, const RootDatum& rd)
{
  latticetypes::Weight v = q.apply(rd.twoRho());

  q.leftMult(rd.matrix(toPositive(v,rd)));
}


/*!
\brief Writes in q the matrix represented by ww.

  NOTE: not intended for heavy use. If that were the case, it would be better
  to use the decomposition of ww into pieces, and multiply those together.
  However, that would be for the ComplexGroup to do, as it is it that has
  access to both the Weyl group and the root datum.
*/
latticetypes::LatticeMatrix RootDatum::matrix(const weyl::WeylWord& ww) const
{
  latticetypes::LatticeMatrix q; matrix::identityMatrix(q,rank());

  for (size_t i=0; i<ww.size(); ++i)
    q *= simple_reflection(ww[i]);

  return q;
}

/*! \brief Writes in q the matrix represented by the product of the
reflections for the set of roots |rset|.

The roots must be mutiually orthogonal so that the order doesn't matter.
*/
latticetypes::LatticeMatrix refl_prod(const RootSet& rset, const RootDatum& rd)
{
  latticetypes::LatticeMatrix q; identityMatrix(q,rd.rank());
  for (RootSet::iterator it=rset.begin(); it(); ++it)
    q *= rd.root_reflection(*it);
  return q;
}


/*!\brief
  Returns a reduced expression of the shortest |w| making |w.v| dominant

  Algorithm: the greedy algorithm -- if v is not positive, there is a
  simple coroot alpha^v such that <v,alpha^v> is < 0; then s_alpha.v takes
  v closer to the dominant chamber.
*/
weyl::WeylWord toPositive(latticetypes::Weight v, const RootDatum& rd)
{
  weyl::WeylWord result;

  size_t j;
  do
    for (j=0; j<rd.semisimpleRank(); ++j)
      if (v.scalarProduct(rd.simpleCoroot(j)) < 0)
      {
	result.push_back(j);
	rd.simpleReflect(v,j);
	break;
      }
  while (j<rd.semisimpleRank());

  // reverse result (action is from right to left)
  std::reverse(result.begin(),result.end());
  return result;
}

RootDatum integrality_datum(const RootDatum& rd,
			    const latticetypes::RatWeight& lambda)
{
  latticetypes::LatticeCoeff n=lambda.denominator();
  const latticetypes::Weight& v=lambda.numerator();
  RootSet int_roots(rd.numRoots());
  for (size_t i=0; i<rd.numPosRoots(); ++i)
    if (v.dot(rd.posCoroot(i))%n == 0)
      int_roots.insert(rd.posRootNbr(i));

  RootList simple = rd.simpleBasis(int_roots);

  latticetypes::WeightList roots;   roots.reserve(simple.size());
  latticetypes::WeightList coroots; coroots.reserve(simple.size());

  for (size_t i=0; i<simple.size(); ++i)
  {
    roots.push_back(rd.root(simple[i]));
    coroots.push_back(rd.coroot(simple[i]));
  }

  return RootDatum(prerootdata::PreRootDatum(roots,coroots,rd.rank()));
}

arithmetic::RationalList integrality_points
  (const RootDatum& rd, latticetypes::RatLatticeElt& nu)
{
  nu.normalize();
  unsigned long d = abs(nu.denominator());

  std::set<long> products;
  for (size_t i=0; i<rd.numPosRoots(); ++i)
  {
    long p = abs(nu.numerator().dot(rd.posCoroot(i)));
    if (p!=0)
      products.insert(p);
  }

  std::set<arithmetic::Rational> fracs;
  for (std::set<long>::iterator it= products.begin(); it!=products.end(); ++it)
  {
    for (long s=d; s<=*it; s+=d)
      fracs.insert(arithmetic::Rational(s,*it));
  }
  return arithmetic::RationalList(fracs.begin(),fracs.end());
}

} // namespace rootdata

/*****************************************************************************

                Chapter III -- Auxiliary methods.

******************************************************************************/

namespace rootdata {



// a class for making a compare object for indices, backwards lexicographic
class weight_compare
{
  const std::vector<latticetypes::Weight>& alpha; // weights being compared
  std::vector<latticetypes::Weight> phi; // coweights by increasing priority

public:
  weight_compare(const std::vector<latticetypes::Weight>& roots,
		 const std::vector<latticetypes::Weight>& f)
    : alpha(roots), phi(f) {}

  void add_coweight(const latticetypes::Weight& f) { phi.push_back(f); }

  bool operator() (size_t i, size_t j)
  {
    latticetypes::LatticeCoeff x,y;
    for (size_t k=phi.size(); k-->0; )
      if ((x=phi[k].scalarProduct(alpha[i]))!=
	  (y=phi[k].scalarProduct(alpha[j])))
	return x<y;

    return false; // weights compare equal under all coweights
  }
}; // |class weight_compare|




} // |namespace rootdata|

} // |namespace atlas|
