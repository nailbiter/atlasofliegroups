#!/usr/bin/perl
#generate graph of inclusions of at files
#usage: file_graph.pl [filename]
# generates dependencies.dot, dependencies.ps, dependencies_reduced.dot, dependencies_reduced.ps (or filename in place of dependencies)

use strict;
use List::MoreUtils qw/uniq/;

my $filename=$ARGV[0]||"dependencies";



my $dotfile="$filename".".dot";
my $dotfile_reduced="$filename"."_reduced.dot";
my $psfile="$filename".".ps";
my $psfile_reduced="$filename"."_reduced.ps";
open(OUT,">$dotfile")||die("Can't open $dotfile for output");

#my $files=`grep -h "^<.*at" *`;
my $files=`ls *at`;
my @files=split "\n", $files;

select(OUT);
print "
digraph G {
ratio=\"1.5\"
size=\"7.5,10!\"";
foreach my $file (@files){
    my $string=$file;
    $string=~ s/\.at//;
    print("\"$string\"\n");
}

foreach my $file (@files){
    my $targets=`grep "^<.*at" $file`;
    $file =~ s/\.at//g;
    my @targets=split "\n", $targets;
    foreach my $target (@targets){
	$target=~ s/<//;
	$target=~ s/{.*//g;
	$target=~ s/\s//g;
	$target=~ s/\.at//;
	print("\n\"$file\"->\"$target\"");
    }
}
print("\n}");
`tred $dotfile > $dotfile_reduced`;

`dot -Tps -o$psfile $dotfile`;
`dot -Tps -o$psfile_reduced $dotfile_reduced`;
close(OUT);








