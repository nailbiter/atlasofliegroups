#!/bin/bash

shopt -s nullglob # don't give us those stupid files named '*'.something

here=$(pwd)
version=$1
distr=atlas_$version
distr_subdirs="messages sources rx-scripts"
distr_files="COPYRIGHT LICENSE README CHANGES"
distr_all_src="error interface io utilities structure gkmod test stand-alone interpreter"
dirs_with_cweb="interpreter stand-alone io"  # directories containing *.w files
rx_files="basic groups parameters K K_types LKT Weylgroup W_orbit \
cross_W_orbit dual finite_dimensional galois generate_groups group_operations \
hermitian induction iterate_deform kl lattice matrix nilpotent nonintegral \
polynomial representations sort tits torus translate twist unitary \
test_unitarity lietypes misc my"
help_files="README.rx-scripts examples" # files *.help are also included

echo Building $distr

rm -rf $distr
mkdir $distr
for sd in $distr_subdirs ; do mkdir $distr/$sd; done
for f in $distr_files ; do ln -s $here/$f $distr; done
ln -s $here/distr_INSTALL $distr/INSTALL
ln -s $here/distr_Makefile $distr/Makefile
ln -s $here/getversion.pl $distr
ln -s $here/messages/* $distr/messages/
ln -s $here/sources/version.h $distr/sources/
for f in $rx_files ; do ln -s $here/rx-scripts/$f.rx $distr/rx-scripts ; done
for f in $help_files ; do ln -s $here/rx-scripts/$f $distr/rx-scripts ; done
ln -s $here/rx-scripts/*.help $distr/rx-scripts/
ln -s $here/doc/modules $distr/
for sd in $distr_all_src
  do mkdir $distr/sources/$sd
     # the following command uses ln -t so that empty expansion at end is
     # just an error (which we shut up), rather than an unintended form of ln
     ln -s -t $distr/sources/$sd/ $here/sources/$sd/*.{h,cpp} 2>/dev/null
  done
ln -s $here/sources/interface/input*readline.c $distr/sources/interface/

for sd in $dirs_with_cweb
do pushd $distr/sources/$sd >/dev/null
  for f in $here/sources/$sd/*.w
  do rm -f $(basename $f .w).{h,cpp} # don't write to symbolic links
     ctanglex ++ -lbhp $f - $(basename $f .w) # no changefile, set output name
  done
  popd >/dev/null
done
(cd $distr/sources/interpreter >/dev/null; bison -d --report=state \
  $here/sources/interpreter/parser.y)

cat sources/*/*.d >$distr/dependencies

tar cvz --file=$distr.tgz --dereference $distr
rm -r $distr
