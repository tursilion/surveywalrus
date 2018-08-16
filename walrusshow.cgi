#!/usr/bin/perl 

print "Content-type: text/html\n\n";

# read and emit the header
open my $file, '/home/tursilion/surveywalrus/header.txt'; print <$file>; close $file;

# link to the voting form
print "Use <font size=\"+1\"><a href=\"https://harmlesslion.com/cgi-bin/walrusquery.cgi\">";
print "https://harmlesslion.com/cgi-bin/walrusquery.cgi</a></font> to vote (once per day)<br>";

# get the generated form
$data=`/home/tursilion/surveywalrus/sw/surveywalrus get_scores html`;
print "<br>\n$data<br>\n";

#read and emit the footer
open $file, '/home/tursilion/surveywalrus/footer.txt'; print <$file>; close $file;
