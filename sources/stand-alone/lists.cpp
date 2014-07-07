#include <vector>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cstring>
#include <cstdlib> // for |exit|
#include <list> // for comparison
#include <stack> // for comparison
#include <queue> // for comparison
#include "sl_list.h"

typedef atlas::tags::IteratorTag IT;


class A
{
  unsigned int a;
public:
  A (unsigned int a) : a(a) { std::cout << "construct " << a << std::endl; }
  A (const A& x) : a(x.a) { std::cout << "copy " << a << std::endl; }
  A& operator = (const A& x)
  { a=x.a; std::cout << "assign " << a << std::endl; return *this; }
  ~A ();
  operator unsigned int () { return a; }
};

typedef std::unique_ptr<A> uA;
uA p;

A::~A()  { std::cout << "destruct " << a
		    << ", p is " << (p.get()==nullptr ? "null" : "not null" )
		    << std::endl; }

void g()
{
  typedef atlas::containers::sl_list<unsigned> List;
  typedef atlas::containers::mirrored_sl_list<unsigned> rev_List;
  typedef std::list<unsigned> STList;
  typedef std::vector<unsigned> vec;

  List a,b,c;
  STList sa,sb,sc;
  vec va,vb,vc;

  a.insert(a.begin(),4);
  a.insert(a.end(),1);
  a.insert(a.begin(),2);

  sa = STList(a.begin(),a.end());
  va = vec(a.begin(),a.end());

  b=a;
  sb=sa;
  vb=va;

  b.insert(b.insert(++b.begin(),7),8);
  sb.insert(sb.insert(++sb.begin(),7),8);
  vb.insert(vb.insert(++vb.begin(),7),8);

  c=List(4,17);
  c.insert(c.end(),b.begin(),b.end(),IT());
  sc=sb;
  vc=vb;

  c.erase(c.begin());
  sc.erase(sc.begin());
  vc.erase(vc.begin());

  List::iterator p=++ ++c.begin();
  STList::iterator sp=++ ++sc.begin();
  vec::iterator vp=++ ++vc.begin();

  a.insert(a.begin(),5);
  sa.insert(sa.begin(),5);
  va.insert(va.begin(),5);

  for (List::const_iterator it=a.begin(); it!=a.end(); ++it)
    c.insert(c.begin(),*it);
  for (STList::const_iterator it=sa.begin(); it!=sa.end(); ++it)
    sc.insert(sc.begin(),*it);
  for (vec::const_iterator it=va.begin(); it!=va.end(); ++it)
    vc.insert(vc.begin(),*it);

  c.erase(p);
  sc.erase(sp);
  // vc.erase(vp); // UB

  b.insert(b.end(),b.begin(),b.end(),IT());
  sb.insert(sb.end(),sb.begin(),sb.end());


  std::ostream_iterator<unsigned long> lister(std::cout,",");
  std::copy (a.begin(), a.end(), lister);
  std::cout << std::endl;
  std::copy (b.begin(), b.end(), lister);
  std::cout << std::endl;
  std::copy (c.begin(), c.end(), lister);
  std::cout << std::endl << std::endl;

  std::copy (sa.begin(), sa.end(), lister);
  std::cout << std::endl;
  std::copy (sb.begin(), sb.end(), lister);
  std::cout << std::endl;
  std::copy (sc.begin(), sc.end(), lister);
  std::cout << std::endl << std::endl;

  std::copy (va.begin(), va.end(), lister);
  std::cout << std::endl;
  std::copy (vb.begin(), vb.end(), lister);
  std::cout << std::endl;
  std::copy (vc.begin(), vc.end(), lister);
  std::cout << std::endl << std::endl;

  typedef std::stack<unsigned,rev_List> Q;
  Q q;
  for (unsigned int i=0; i<8; ++i)
  {
    q.push(i); q.push(i*i); q.pop();
  }
  while (not q.empty())
  {
    std::cout << q.top() << ", ";
     q.pop();
  }
  std::cout << std::endl;
}

typedef atlas::containers::simple_list<int> intlist;
typedef atlas::containers::sl_list<int> int_list;

std::ostream& operator << (std::ostream& os, const intlist & l)
{
  os << '[';
  for (auto it = l.begin(); not l.at_end(it); ++it)
    os << (it==l.begin() ? "" : ",") << *it;
  return os << ']';
}

int main()
{
  std::ostream_iterator<int> lister(std::cout,", ");
  intlist a { 13,4,1,3 };

  std::cout << a << "; ";
  a.reverse();
  std::cout << a << std::endl;

  a.reverse(a.begin(),++++++a.begin());
  std::cout << a << std::endl;

  int_list b(std::move(a));
  std::cout << a << std::endl;
  std::copy(b.begin(),b.end(),lister); std::cout<<std::endl;
  b.push_back(17);
  std::copy(b.begin(),b.end(),lister); std::cout<<std::endl;
  a = b.undress();
  std::cout << a << std::endl;
  std::copy(b.begin(),b.end(),lister); std::cout<<std::endl;
}