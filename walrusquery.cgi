#!/usr/bin/perl 

print "Content-type: text/html\n\n";

# read and emit the header
open my $file, '/home/tursilion/surveywalrus/header.txt'; print <$file>; close $file;

# provide a bypass
print '<a href="/cgi-bin/walrusshow.cgi">... or click here to see results without voting ...</a><br>';

# get the generated form
$data=`/home/tursilion/surveywalrus/sw/surveywalrus generate_table`;
print "<br>\n$data<br>\n";

#read and emit the footer
open $file, '/home/tursilion/surveywalrus/footer.txt'; print <$file>; close $file;
