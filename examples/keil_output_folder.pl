#!/usr/bin/perl

use File::Spec;
use File::Find;
use File::Path;
use File::Glob;
use File::stat;
#use Time::Piece;
#use Time::Seconds;

$" = "\n"; # change list separator
@PROJECT_LIST = (<device/device*/*.uvproj>, <host/host*/*.uvproj>);
print "@PROJECT_LIST";

foreach (@PROJECT_LIST)
{
  my $project_file = $_;
  my $backup_file = $project_file . ".bck";
 
  rename $project_file, $backup_file or die "cannot rename $project_file to $backup_file";
  
  open (fin, $backup_file) or die "Can't open $backup_file to read\n";
  open (fout, ">$project_file") or die "Can't open $project_file to write\n";
  
  my $target;
  while (<fin>)
  {
    s/(<TargetName>.+) /\1_/; # replace space by underscore in target name if found
  
    $target = $1 and print $target . "\n" if /<TargetName>(.+)</;
    my $keil_build = ".\\KeilBuild\\$target\\";
    
    print "replace $2 by $keil_build\n--> $_\n" if s/(<OutputDirectory>)(.+)</\1$keil_build</ || s/(<ListingPath>)(.+)</\1$keil_build</;
    
    printf fout;
  }
}