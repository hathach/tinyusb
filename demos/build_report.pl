#!/usr/bin/perl

use Scalar::Util qw(looks_like_number);

$" = "\n"; # change list separator

$keil_size = "Program Size:.+";

%report_patterns = 
( #toolchain, pattern-list
  'keil'    => ['Build target \'(.+)\'', '(\d+ Error.+\d+ Warning)', $keil_size . 'Code=(\d+)', $keil_size . 'RO-data=(\d+)', $keil_size . 'RW-data=(\d+)', $keil_size . 'ZI-data=(\d+)'],
  'iar'     => ['Building configuration.+ (.+)', 'Total number of (.+)', '((\s+\d+){4})\s+[0-9a-f]+'],
  'xpresso' => ['Build of configuration (\S+) ', '(Finished) building target', '((\s+\d+){4})\s+[0-9a-f]+']
);

@report_file_list = <build_all_*.txt>;
#print "@report_file_list"; die;

open $freport, ">build_report.txt" or die "cannot open build_reprot.txt";

foreach (@report_file_list)
{
  /build_all_([^_]+)_/;
  build_report($_, $1);
}

sub build_report 
{
  my $report_file = $_[0];
  my $toolchain   = $_[1];
  
  my @pattern     = @{$report_patterns{$toolchain}};
  
  open $report_handle, $report_file or die "cannot open $report_file";
  
  $report_file =~ /build_all_(.+).txt/;
  
  print $freport "--------------------------------------------------------------------\n";
  printf $freport "%-25s", $1;
  printf $freport "%13s", "" if $toolchain eq 'iar';
  print $freport "            text	   data	    bss	    dec" if $toolchain eq 'xpresso' or $toolchain eq 'iar';
  print $freport "  Code     RO     RW     ZI" if $toolchain eq 'keil';
  print $freport "\n--------------------------------------------------------------------";

  while( my $line = <$report_handle> )
  {
    local $/ = "\r\n";
    chomp $line;
    
    foreach (@pattern)
    {
      if ($line =~ /$_/)
      {
        my $fmat = ($_ eq $pattern[0]) ? "\n%-25s" :  "%s ";
        $fmat = "%6s " if $toolchain eq 'keil' and looks_like_number($1);
        printf $freport $fmat, $1; 
      }
    }
  }
  
  close $report_handle;
  
  print $freport "\n\n";
}