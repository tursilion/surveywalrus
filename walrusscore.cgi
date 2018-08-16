#!/usr/bin/perl 
# SurveyWalrus - commit voting result, if any, and show results
# We also need to ignore multiple votes from one source

use strict; 
use warnings;
use CGI;
use IPC::Run3 qw(run3);
use List::Util qw(max);

# use the 15-year-new CGI hotness...
my $query = CGI->new;

# now just truncate the first non-alnum character
#$value =~ s/[^a-zA-Z0-9].*$//;
#$name =~ s/[^a-zA-Z0-9].*$//;

# generate a header so that we can emit output
print "Content-type: text/html\n\n";

# read and emit the header
open my $file, '/home/tursilion/surveywalrus/header.txt'; print <$file>; close $file;

# $ENV{'REMOTE_ADDR'} isn't perfect, proxies can change it, but it'll do
my $srcip = $ENV{'REMOTE_ADDR'};
if (!$srcip) { $srcip=''; }
$srcip =~ s/[^0-9\.].*$//;    # truncate at the first non-IPv4 character
my @app = ('/home/tursilion/surveywalrus/sw/surveywalrus', 'add_vote', $srcip);

# now we need to build an array of lines for the STDIN of the app
my $sin = '';
my $sout = '';
my $serr = '';

# the inputs should be a list of keys xxxx_yyyy and values '1' if present
my @scores;
my @names = $query->param;
my $cnt = 0;
foreach my $key (@names) {
    if ($key =~ m/^[0-9]{4}_[0-9]{4}$/) {
        # this looks like one of ours
        (my $idx, my $vote) = split(/_/, $key);
        $scores[$idx][$vote]=1;
        ++$cnt;
    }
}

if ($cnt == 0) {
  print "<!-- no votes were passed -->\n";
} else {
  # now we just need to dump the scores array into $sin
  foreach my $line (0 .. $#scores ) {
      $sin .= $line . ' ';
      foreach my $i (0 .. $#{$scores[$line]} ) {
          if ($scores[$line][$i]) {
              $sin .= '1 ';
          } else {
              $sin .= '0 ';
          }
      }
      # to avoid an empty line, it's always safe to add an extra 0
      $sin .= "0\n";
  }

  # now pass it to the walrus
  run3 \@app, \$sin, \$sout, \$serr;

  # and emit the results
  print "<!-- $sout -->\n";
}

print "<br>Use <font size=\"+1\"><a href=\"https://harmlesslion.com/cgi-bin/walrusshow.cgi\">";
print "https://harmlesslion.com/cgi-bin/walrusshow.cgi</a></font> to share results<br>\n";

# this is essentially the same as walrusshow.cgi...
my $data=`/home/tursilion/surveywalrus/sw/surveywalrus get_scores html`;
print "<br>\n$data<br>\n";

#read and emit the footer
open $file, '/home/tursilion/surveywalrus/footer.txt'; print <$file>; close $file;

