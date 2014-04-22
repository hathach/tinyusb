#!/usr/bin/perl

################## HOW TO USE THIS FILE #####################
# iar keil xpresso to build with those toolchain
# clean or build for action
#############################################################
use List::MoreUtils 'any';
use File::Spec;
use File::Find;
use File::Path;
use File::Glob;
use File::stat;
use File::Basename;
use Cwd;
use Cwd 'abs_path';

#use Time::Piece;
#use Time::Seconds;

$" = "\n"; # change list separator

$KEIL_PATH    = 'C:/Keil/UV4'; #'/C/Keil/UV4';
$IAR_PATH     = glob ('C:/Program*/IAR*/Embedded*/common/bin');
$XPRESSO_PATH = glob ('C:/nxp/LPCXpresso_7*/lpcxpresso');
$XPRESSO_PATH = "$XPRESSO_PATH;$XPRESSO_PATH/bin;$XPRESSO_PATH/tools/bin;$XPRESSO_PATH/msys/bin";

$ENV{'PATH'} = $KEIL_PATH . ';' . $IAR_PATH . ';' . $XPRESSO_PATH . ';' . $ENV{'PATH'};
#print $ENV{'PATH'}; die;

$repo_path = abs_path(cwd . "/.."); 
#print $repo_path; die;

$device_dir = "device/device";
$host_dir   = "host/host";

$is_build = any { /build/ } @ARGV;
$is_clean = any { /clean/ } @ARGV;
$is_build = 1 if !$is_clean; # default is build

$is_keil     = (any { /keil/    } @ARGV) || (any { /all/ } @ARGV);
$is_iar      = (any { /iar/     } @ARGV) || (any { /all/ } @ARGV);
$is_xpresso  = (any { /xpresso/ } @ARGV) || (any { /all/ } @ARGV);

################## KEIL #####################
if ($is_keil)
{
  @KEIL_PROJECT_LIST = (<$device_dir*/*.uvproj>, <$host_dir*/*.uvproj>);

  foreach (@KEIL_PROJECT_LIST)
  {
    /([^\/]+).uvproj/;
    my $log_file = "build_all_keil_" . $1 . ".txt";
    my $build_cmd = "Uv4 -b $_ -z -j0 -o ../../$log_file";

    cmd_execute($build_cmd);
  }
}

################## IAR #####################
if ($is_iar)
{
  @IAR_PROJECT_LIST = (<$device_dir*/*.ewp>, <$host_dir*/*.ewp>);

  foreach (@IAR_PROJECT_LIST)
  {
    my $proj_dir = dirname $_;
     
    /([^\/]+).ewp/;
    my $proj_name = $1;
    my $log_file = "build_all_iar_" . $proj_name . ".txt";
    unlink $log_file; #delete log_file if existed

    #open project file to get configure name
    my $file_content = file_to_var($_);
    
    #get configure by pattern and build
    while ($file_content =~ /^\s*<configuration>\s*$^\s*<name>(.+)<\/name>\s*$/gm)
    {
      my $build_cmd = "IarBuild $_ -build $1 -log warnings >> $log_file";
      cmd_execute($build_cmd);

      my $out_file = "$proj_dir/$1/Exe/$proj_name.out";
      system("size $out_file >> $log_file");
    }  
  }
}

################## LPCXPRESSO #####################
($repo_path_other_dash = $repo_path) =~ s/\//\\/g;
if ($is_xpresso)
{
  $workspace_dir = "C:/Users/hathach/Dropbox/tinyusb/workspace7"; #projects must be opened in the workspace to be built
  @XPRESSO_PROJECT_LIST = (<$device_dir*/.cproject>, <$host_dir*/.cproject>);

  foreach (@XPRESSO_PROJECT_LIST)
  {
    /([^\/]+)\/.cproject/;
    my $log_file = "build_all_xpresso_" . $1 . ".txt";
    my $build_cmd = "lpcxpressoc -nosplash --launcher.suppressErrors -application org.eclipse.cdt.managedbuilder.core.headlessbuild -cleanBuild $1 -data $workspace_dir > $log_file";
    
    cmd_execute($build_cmd);
    
    #open log file to clean up output
    open (my $fin, $log_file) or die;
    my @log_content = <$fin>;
    close($fin);
    
    open (my $fout, ">$log_file") or die;
    
    foreach (@log_content)
    {
      unless (/Invoking: MCU C Compiler/ or /arm-none-eabi-gcc -D/ or /Finished building:/ or /^ $/)
      {
        s/Building file:.+?([^\/]+\.[ch])/\1/;
        s/$repo_path//;
        s/$repo_path_other_dash//;
        print $fout $_;
      }
    }
  }
}

### call report builder ###
system("perl build_report.pl");

################## HELPER #####################
sub cmd_execute 
{
  print "executing: $_[0]\n...";
  system($_[0]);
  print "done\n";
}

sub file_to_var
{ #open project file to get configure name
  my $file_content;
  open(my $fin, $_[0]) or die "Can't open $_[0] to read\n";
  {
    local $/;
    $file_content = <$fin>;
    close($fin);
  }
  
  return $file_content;
}

sub var_to_file
{ # file name, content
  open(my $fout, ">$_[0]") or die "Can't open $_[0] to write\n";
  {
    print $fout $_[1];
    close($fout);
  }
}
