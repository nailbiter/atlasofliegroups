/*
  This is interactive.cpp

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Reductive Lie Groups

  See file main.cpp for full copyright notice
*/

#include "interactive.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include <map>

#include "basic_io.h"
#include "bitmap.h"
#include "prerootdata.h"
#include "complexredgp.h"
#include "complexredgp_io.h"
#include "ioutils.h"
#include "realform_io.h"
#include "realredgp.h"
#include "realredgp_io.h"
#include "rootdata.h"

#include "interactive_lattice.h"
#include "interactive_lietype.h"

#ifndef NREADLINE
#include <readline/readline.h>
#endif

#include "input.h"

/*****************************************************************************

Preliminaries

******************************************************************************/

namespace atlas {

namespace interactive {

  using namespace interactive_lattice;
  using namespace interactive_lietype;

}

namespace {

  using namespace std;
  using namespace interactive;

  input::InputBuffer inputBuf;

  bool checkInvolution(latticetypes::LatticeMatrix&,
		       const latticetypes::WeightList&);

  bool checkInvolution(const latticetypes::LatticeMatrix&,
		       const layout::Layout&);
}

/*****************************************************************************

        Chapter I -- The OutputFile and InputFile classes

  This was moved here from the ioutils module, for reasons explained in the
  file ioutils.h

  This classes implement is a well-known C++ trick : a file which is opened by
  its constructor, and closed by its destructor.

******************************************************************************/

namespace ioutils {

OutputFile::OutputFile() throw(error::InputError)
{
  std::string name=interactive::getFileName
    ("Name an output file (return for stdout, ? to abandon): ");

  if (name.empty()) {
    d_foutput = false;
    d_stream = &std::cout;
  }
  else {
    d_foutput = true;
    d_stream = new std::ofstream(name.c_str());
  }
}


OutputFile::~OutputFile() // Closes *d_stream if it is not std::cout.
{
  if (d_foutput)
    delete d_stream;
}

InputFile::InputFile(std::string prompt, std::ios_base::openmode mode)
   throw(error::InputError)
{
  // temporarily deactivate completion: default to file-name completion
#ifndef NREADLINE
  rl_compentry_func_t* old_completion_function = rl_completion_entry_function;
  rl_compdisp_func_t * old_hook = rl_completion_display_matches_hook;
  rl_completion_entry_function = NULL;
  rl_completion_display_matches_hook = NULL;

  bool error=false;

  try {
#endif
    do {
      std::string name=interactive::getFileName
	("Give input file for "+ prompt+" (? to abandon): ");
      d_stream = new std::ifstream(name.c_str(),mode);
      if (d_stream->is_open())
	break;
      std::cout << "Failure opening file, try again.\n";
    } while(true);
#ifndef NREADLINE
  }
  catch (error::InputError) {
    error=true;
  }
  rl_completion_entry_function = old_completion_function;
  rl_completion_display_matches_hook = old_hook;
  if (error)
    throw error::InputError();
#endif
}

InputFile::~InputFile() { delete d_stream; }

} // namespace ioutils

/*****************************************************************************

        Chapter II -- Functions declared in interactive.h

******************************************************************************/

namespace interactive {

/*
  Synopsis: get a file name from terminal, abandon with InputError on '?'
*/

std::string getFileName(std::string prompt)
  throw(error::InputError)
{
  input::InputBuffer buf;

  buf.getline(std::cin, prompt.c_str(), false); // get line, no history

  if (hasQuestionMark(buf))
    throw error::InputError();

  std::string name; buf >> name; return name;
}


void bitMapPrompt(std::string& prompt, const char* mess,
		  const bitmap::BitMap& vals)

/*
  Synopsis: puts in prompt an informative string for getting a value from vals.

  Explanation: the string is actually "mess (one of ...): ", where ... stands
  for the set elements in vals.
*/

{
  using namespace basic_io;

  std::ostringstream values;
  seqPrint(values,vals.begin(),vals.end());

  prompt = mess;
  prompt.append(" (one of ");
  prompt.append(values.str());
  prompt.append("): ");

  return;
}

void getCartanClass(size_t& cn, const bitmap::BitMap& cs,
		    input::InputBuffer& line)
  throw(error::InputError)

/*
  Synopsis: gets a cartan class number from the user.

  Explanation: cs contains the set of cartans defined for some real form;
  the user must enter one of those. Before getting the value from the user,
  we attempt to read it from line (this allows for type-ahead.)

  Throws an InputError if not successful; cn is not touched in that case.
*/

{
  // if there is a unique cartan class, choose it
  if (cs.size() == 1) {
    std::cout << "there is a unique conjugacy class of Cartan subgroups"
	      << std::endl;
    cn = 0;
    return;
  }

  // get value from the user
  std::string prompt;
  bitMapPrompt(prompt,"cartan class",cs);
  unsigned long c;
  getInteractive(c,prompt.c_str(),cs,&line); // may throw an InputError
  // commit
  cn = c;

  return;
}

void getInnerClass(latticetypes::LatticeMatrix& d,
		   layout::Layout& lo)
  throw(error::InputError)

/*
  Synopsis: puts in d a distinguished involution for prd.

  Precondition: lo contains the lie type and lattice basis resulting from
  the interactive construction of the group.

  Writes the inner class in lo.

  An inner class is described by a sequence of typeletters, one of:

    - c : compact (d is the identity);
    - s : split (d is minus the identity);
    - C : complex (d flips two consecutive isomorphic factors in lt);
    - u : the non-identity inner form in type D_{2n}; there are actually
          three of these for D4 but they are conjugate in Out(G), so it is
	  enough to consider one of them.

  There have to be enough letters to deal with all of lt (a C consumes two
  entries in lt). Moreover, the inner class has to be compatible with the
  chosen lattice, i.e., d has to stabilize the lattices generated by the
  roots and the coroots.

  Throws an InputError if the interaction is not successful.
*/

{
  using namespace latticetypes;
  using namespace lietype;

  const LieType& lt = lo.d_type;

  InnerClassType ict;
  getInteractive(ict,lt); //< may throw an InputError

  LatticeMatrix i;
  involution(i,lt,ict);

  while (not checkInvolution(i,lo)) { // reget the inner class
    std::cerr
      << "sorry, that inner class is not compatible with the weight lattice"
      << std::endl;
    getInteractive(ict,lt);
    involution(i,lt,ict);
  }

  lo.d_inner = ict;

  // construct the matrix
  LatticeMatrix p(lo.d_basis);

  d = i;
  invConjugate(d,p);

  return;
}

void getInteractive(lietype::LieType& d_lt) throw(error::InputError)

/*
  Synopsis: gets a LieType interactively from the user.

  Throws an InputError is the interaction does not end in a correct assignment
  of lt. Does not touch lt unless the assignment succeeds.
*/

{
  using namespace error;
  using namespace input;
  using namespace lietype;

  HistoryBuffer hb;
  hb.getline(std::cin,"Lie type: ");

  if (hasQuestionMark(hb))
    throw InputError();

  while (not checkLieType(hb)) { // retry
    hb.getline(std::cin,"Lie type (? to abort): ");
    if (hasQuestionMark(hb))
      throw InputError();
  }

  // if we reach this point, the type is correct

  inputBuf.str(hb.str());
  inputBuf.reset();

  LieType lt;
  readLieType(lt,inputBuf);
  d_lt.swap(lt);

  return;
}

void getInteractive(lietype::InnerClassType& ict, const lietype::LieType& lt)
  throw(error::InputError)

/*
  Synopsis: gets an InnerClassType interactively from the user.

  Throws an InputError is the interaction does not end in a correct assignment
  of ict. Does not touch ict unless the assignment succeeds.
*/

{
  using namespace error;
  using namespace input;
  using namespace lietype;

  if (checkInnerClass(inputBuf,lt,false))
    goto read;

  inputBuf.getline(std::cin,"enter inner class(es): ",false);

  if (hasQuestionMark(inputBuf))
    throw InputError();

  while (not checkInnerClass(inputBuf,lt)) { // retry
    inputBuf.getline(std::cin,"enter inner class(es) (? to abort): ",false);
    if (hasQuestionMark(inputBuf))
      throw InputError();
  }

 read:
  readInnerClass(ict,inputBuf,lt);

  return;
}

void getInteractive(prerootdata::PreRootDatum& d_prd,
		    latticetypes::WeightList& d_b,
		    const lietype::LieType& lt)
  throw(error::InputError)

/*
  Replaces prd with a new PreRootDatum gotten interactively from the user.
  The Lie type is given in lt.

  The list b is used to make a note of the base change from the original
  simple weight basis associated to the standard "simply connected times torus"
  form of lt to the actual lattice basis; this might be necessary for checking
  if certain real forms are defined for this covering.

  Throws an InputError if the interaction with the user fails. In that case,
  d_b and d_prd are not touched.
*/

{
  using namespace latticetypes;
  using namespace prerootdata;

  // get lattice basis

  WeightList b;
  CoeffList invf;

  smithBasis(invf,b,lt);
  getLattice(invf,b);   // may throw an InputError

  d_b.swap(b);

  // make new PreRootDatum

  PreRootDatum prd(lt,d_b);

  // swap prd and d_prd; the old PreRootDatum will be destroyed on exit

  d_prd.swap(prd);

  return;
}

void getInteractive(realform::RealForm& d_rf,
		    const complexredgp_io::Interface& I)
  throw(error::InputError)

/*
  Synposis: replaces rf with a new real form gotten interactively from the
  user.

  Throws an InputError if the interaction with the user fails; in that case,
  rf is not modified.
*/

{
  using namespace realform_io;

  const realform_io::Interface rfi = I.realFormInterface();

  // if there is only one choice, make it
  if (rfi.numRealForms() == 1) {
    std::cout << "there is a unique real form: " << rfi.typeName(0)
	      << std::endl;
    d_rf = 0;
    return;
  }

  // else get choice from user
  std::cout << "(weak) real forms are:" << std::endl;
  for (size_t j = 0; j < rfi.numRealForms(); ++j) {
    std::cout << j << ": " << rfi.typeName(j) << std::endl;
  }

  unsigned long r;
  getInteractive(r,"enter your choice: ",rfi.numRealForms());

  d_rf = rfi.in(r);

  return;
}

void getInteractive(realform::RealForm& rf,
		    const complexredgp_io::Interface& I,
		    tags::DualTag)
  throw(error::InputError)

/*
  Synposis: replaces rf with a new dual real form gotten interactively from
  the user.

  Throws an InputError if the interaction with the user fails; in that case,
  rf is not modified.
*/

{
  using namespace realform_io;

  const realform_io::Interface rfi = I.dualRealFormInterface();

  // if there is only one choice, make it
  if (rfi.numRealForms() == 1) {
    std::cout << "there is a unique dual real form: " << rfi.typeName(0)
	      << std::endl;
    rf = 0;
    return;
  }

  // else get choice from user
  std::cout << "(weak) dual real forms are:" << std::endl;
  for (size_t j = 0; j < rfi.numRealForms(); ++j) {
    std::cout << j << ": " << rfi.typeName(j) << std::endl;
  }

  unsigned long r;
  getInteractive(r,"enter your choice: ",rfi.numRealForms());

  rf = rfi.in(r);

  return;
}

void getInteractive(realform::RealForm& rf,
		    const complexredgp_io::Interface& I,
		    const realform::RealFormList& drfl,
		    tags::DualTag)
  throw(error::InputError)

/*
  Synposis: replaces rf with a new real form from the list drfl.

  Throws an InputError if the interaction with the user fails; in that case,
  rf is not modified.
*/

{
  using namespace bitmap;
  using namespace realform;
  using namespace realform_io;

  const realform_io::Interface rfi = I.dualRealFormInterface();

  // if there is only one choice, make it
  if (drfl.size() == 1) {
    RealForm rfo = rfi.out(drfl[0]);
    std::cout << "there is a unique dual real form choice: "
	      << rfi.typeName(rfo) << std::endl;
    rf = drfl[0];
    return;
  }

  // else get choice from user
  BitMap vals(rfi.numRealForms());
  for (size_t j = 0; j < drfl.size(); ++j)
    vals.insert(rfi.out(drfl[j]));

  std::cout << "possible (weak) dual real forms are:" << std::endl;
  BitMap::iterator vals_end = vals.end();
  for (BitMap::iterator i = vals.begin(); i != vals_end; ++i) {
    std::cout << *i << ": " << rfi.typeName(*i) << std::endl;
  }

  unsigned long r;
  getInteractive(r,"enter your choice: ",vals);

  rf = rfi.in(r);

  return;
}

void getInteractive(realredgp::RealReductiveGroup& d_G,
		    complexredgp_io::Interface& CI)
  throw(error::InputError)

/*
  Synopsis: replaces d_G by a new RealReductiveGroup gotten
  interactively from the user. The complex group interface is not touched.

  Throws an InputError if the interaction with the user is not successful;
  in that case, d_G is not touched.
*/

{
  using namespace error;
  using namespace realredgp;

  realform::RealForm rf = 0;
  getInteractive(rf,CI); // may throw an InputError

  // commit
  RealReductiveGroup G(CI.complexGroup(),rf);
  d_G.swap(G);

  return;
}

void getInteractive(complexredgp_io::Interface& d_I)
  throw(error::InputError)

/*
  Synopsis: Replaces d_I by a new Interface gotten interactively from the
  user.

  Throws an InputError if the interaction with the user is not successful;
  in that case, d_I is not touched.

  NOTE: here it is assumed that at most one interface is looking at a given
  complex reductive group. If there could be several, some kind of reference
  counting must be used to avoid messing up other people's view.
*/

{
  using namespace complexredgp;
  using namespace complexredgp_io;
  using namespace latticetypes;
  using namespace layout;
  using namespace lietype;
  using namespace prerootdata;
  using namespace rootdata;

  ComplexReductiveGroup& d_G = d_I.complexGroup();

  LieType lt;
  getInteractive(lt);  // may throw an InputError

  WeightList b;

  PreRootDatum prd;
  getInteractive(prd,b,lt); // may throw an InputError

  Layout lo(lt,b);

  LatticeMatrix d;
  getInnerClass(d,lo); // may throw an InputError

  // commit
  RootDatum* rd = new RootDatum(prd);
  ComplexReductiveGroup G(rd,d);
  d_G.swap(G);

  Interface I(d_G,lo);
  d_I.swap(I);

  return;
}

void getInteractive(unsigned long& d_c, const char* prompt, unsigned long max)
  throw(error::InputError)

/*
  Synopsis: gets a value for c from the user, with c in [0,max[.

  Prompts (the first time) with prompt. Regets the value if not in the right
  range. The value is unchanged in case of failure.

*/

{
  using namespace error;
  using namespace input;
  using namespace ioutils;

  InputBuffer ib;
  ib.getline(std::cin,prompt,false);

  skipSpaces(ib);
  if (hasQuestionMark(ib))
    throw InputError();

  unsigned long c;

  if (isdigit(ib.peek()))
    ib >> c;
  else
    c = max;

  while (c >= max) {
    std::cout << "sorry, value must be between 0 and " << max-1 << std::endl;
    ib.getline(std::cin,"try again (? to abort): ",false);
    skipSpaces(ib);
    if (hasQuestionMark(ib))
      throw InputError();
    if (isdigit(ib.peek()))
      ib >> c;
    else
      c = max;
  }

  // commit
  d_c = c;

  return;
}

void getInteractive(unsigned long& d_c, const char* prompt,
		    const bitmap::BitMap& vals, input::InputBuffer* linep)
  throw(error::InputError)

/*
  Synopsis: gets a value for d_c from the user, from the set vals; tries first
  to get from line.

  Prompts (the first time) with prompt. Regets the value if not in the right
  range. The value is unchanged in case of failure
*/

{
  using namespace basic_io;
  using namespace input;
  using namespace ioutils;
  using namespace error;

  unsigned long c;

  if (linep) { // try to get value from line

    InputBuffer& line = *linep;
    skipSpaces(line);

    if (isdigit(line.peek())) {
      line >> c;
      if (vals.isMember(c)) { // we are done
	d_c = c;
	return;
      }
    }

  }

  InputBuffer ib;
  ib.getline(std::cin,prompt,false);

  skipSpaces(ib);

  if (hasQuestionMark(ib))
    throw InputError();

  if (isdigit(ib.peek()))
    ib >> c;
  else
    c = vals.capacity();

  while (not vals.isMember(c)) {
    std::cout << "sorry, value must be one of ";
    seqPrint(cout,vals.begin(),vals.end()) << std::endl;
    ib.getline(std::cin,"try again (? to abort): ",false);
    skipSpaces(ib);
    if (hasQuestionMark(ib))
      throw InputError();
    if (isdigit(ib.peek()))
      ib >> c;
    else
      c = vals.capacity();
  }

  // commit
  d_c = c;

  return;
}

input::InputBuffer& inputLine()

/*
  Synopsis: returns inputBuf.

  This is used to pass parameters to functions: some functions will attempt
  to read from inputLine() before re-getting it.
*/

{
  return inputBuf;
}

} // namespace interactive

/*****************************************************************************

        Chapter III -- Auxiliary functions

******************************************************************************/

namespace {

bool checkInvolution(const latticetypes::LatticeMatrix& i,
		     const layout::Layout& lo)

/*
  Synopsis: checks whether the inner class described in ict is compatible with
  the weight lattice.

  Precondition: lo contains the lie type and lattice basis resulting from
  the interactive construction of the group.
*/

{
  using namespace latticetypes;

  const WeightList& b = lo.d_basis;

  LatticeMatrix p(b);

  // write d.p^{-1}.i.p

  LatticeMatrix m(p);
  leftProd(m,i);
  LatticeCoeff d;
  p.invert(d);
  leftProd(m,p);

  // now i stabilizes the lattice iff d divides m

  return m.divisible(d);
}

} // namespace

} // namespace atlas
