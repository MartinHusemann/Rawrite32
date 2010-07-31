#! /usr/pkg/bin/perl -w

open(IN, "<../www/help/rawrite32-en.htm") || die("could not open input file");
open(OUT, ">./rawrite32-en.htm") || die("could not open output file");
while(<IN>) {
	s/\<\!--- for help: cut start ---\>/\<\!--- for help: cut start/o;
	s/\<\!--- for help: insert start/\<\!--- for help: insert start ---\>/o;
	print OUT "$_";
}
close(IN);
