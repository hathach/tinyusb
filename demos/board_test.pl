#!/usr/bin/perl

use List::MoreUtils 'any';
use List::MoreUtils 'first_value';
use File::Spec;
use File::Find;
use File::Path;
use File::Glob;
use File::stat;
use Cwd;
use Cwd 'abs_path';

$" = "\n"; # change list separator

$KEIL_PATH        = 'C:/Keil/UV4'; #'/C/Keil/UV4';
$IAR_PATH         = glob ('C:/Program*/IAR*/Embedded*/common/bin');

$XPRESSO_PATH     = glob ('C:/nxp/LPCXpresso_7*/lpcxpresso');
$XPRESSO_BIN_PATH = "$XPRESSO_PATH/bin";
$XPRESSO_PATH     = "$XPRESSO_PATH;$XPRESSO_PATH/bin;$XPRESSO_PATH/tools/bin;$XPRESSO_PATH/msys/bin";

$ENV{'PATH'} .= ';' . $KEIL_PATH . ';' . $IAR_PATH . ';' . $XPRESSO_PATH;
$ENV{'PATH'} .= ';' . "C:/Keil/ARM/BIN";
#print $ENV{'PATH'}; die;

$repo_path = abs_path(cwd . "/.."); 

$device_dir = "device/";
$host_dir   = "host/";

##### Command line arguments ###
$board            = $ARGV[0];

$is_keil          = (any { /keil/ or /all/    } @ARGV);
$is_iar           = (any { /iar/ or /all/     } @ARGV);
$is_xpresso       = (any { /xpresso/ or /all/ } @ARGV);
$is_download_only = (any { /download_only/    } @ARGV);

if ( any { /device_/ or /host_/ } @ARGV)
{
  my $build_project;
  $device_dir .= defined ($build_project = first_value { /device_/ } @ARGV) ? $build_project : "nowhere_path" ;
  $host_dir   .= defined ($build_project = first_value { /host_/   } @ARGV) ? $build_project : "nowhere_path";
}else
{ #default is all
  $device_dir .= "*";
  $host_dir   .= "*";
}

#print "$device_dir $host_dir"; die;

my $log_file = "board_$board.txt";
unlink $log_file;
  
################## KEIL #####################
if ($is_keil)
{
  @KEIL_PROJECT_LIST = (<$device_dir*/*.uvproj>, <$host_dir*/*.uvproj>);
  
  foreach (@KEIL_PROJECT_LIST)
  {
    /([^\/]+).uvproj/;
    print_title("Keil $1");

    my $temp_log = "temp_log.txt";
    my $build_cmd = "Uv4 -b $_ -t$board -j0 -o ../../$temp_log";

    if ( $is_download_only || cmd_execute($build_cmd) < 2 )
    {
      append_file($log_file, $temp_log);
      
      my $flash_cmd = "Uv4 -f $_ -t$board -j0 -o ../../$temp_log";
      append_file($log_file, $temp_log) if flash_to_board($flash_cmd);        
    }
  }
}

################## IAR #####################
if ($is_iar)
{
  @IAR_PROJECT_LIST = (<$device_dir*/*.ewp>, <$host_dir*/*.ewp>);

  foreach (@IAR_PROJECT_LIST)
  {  
    /(.+\/)([^\/]+).ewp/;
    print_title("IAR $2");
    
    my $build_cmd = "IarBuild $_ -build $board -log warnings >> $log_file";
    if ( $is_download_only || cmd_execute($build_cmd) == 0)
    {
      my $flash_cmd = "cd $1 & " . iar_flash_cmd($_);
      flash_to_board($flash_cmd);
    }
  }
}

################## LPCXPRESSO #####################
if ($is_xpresso)
{
  (my $repo_path_other_dash = $repo_path) =~ s/\//\\/g;
  my $workspace_dir = "C:/Users/hathach/Dropbox/tinyusb/workspace7"; #projects must be opened in the workspace to be built
  
  my %flash_tool = 
  ( # board => (tool, chip_name)
    'Board_EA4357'         => ['crt_emu_lpc18_43_nxp' , 'LPC4357' ],
    'Board_NGX4330'        => ['crt_emu_lpc18_43_nxp' , 'LPC4330' ],
    'Board_LPCXpresso1769' => ['crt_emu_cm3_nxp'      , 'LPC1769' ],
    'Board_LPCXpresso1347' => ['crt_emu_lpc11_13_nxp' , 'LPC1347' ],
    'Board_rf1ghznode'     => ['crt_emu_lpc11_13_nxp' , 'LPC11U37/401'],
  );
  
  die "board is not supported" unless $flash_tool{$board};
  
  print "all projects in $workspace_dir must be opened and set to the correct MCU of the boards. Enter to continue:\n";
  #<STDIN>;
  
  @XPRESSO_PROJECT_LIST = (<$device_dir*/.cproject>, <$host_dir*/.cproject>);

  foreach (@XPRESSO_PROJECT_LIST)
  {
    /(.+\/(.+))\/.cproject/;
    my $proj_dir = $1;
    my $proj = $2;
    print_title("XPRESSO $proj");
    
    my $build_cmd = "lpcxpressoc -nosplash --launcher.suppressErrors -application org.eclipse.cdt.managedbuilder.core.headlessbuild -build $proj/$board -data $workspace_dir >> $log_file";
    
    system("cd $proj_dir/$board & make clean >> $log_file") if !$is_download_only; # lpcxpresso have a bug that clean the current active config instead of the passed in, manual clean
    
    if ( $is_download_only || cmd_execute($build_cmd) == 0)
    {
      my $flash_cmd = "$flash_tool{$board}[0] -p$flash_tool{$board}[1] -s2000 -flash-load-exec=$proj_dir/$board/$proj.axf";
      
      $flash_cmd .= " -flash-driver=$XPRESSO_BIN_PATH/Flash/LPC18_43_SPIFI_4MB_64KB.cfx" if $board eq 'Board_NGX4330';
      
      #print $flash_cmd; die;
      flash_to_board($flash_cmd);
    }
  }
  
=pod
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
=cut
}

################## HELPER #####################
sub cmd_execute
{
  print "executing: $_[0] ...";
  $result = system($_[0]);
  print "$result done\n";
  return $result;
}

sub flash_to_board
{
  my $flash_cmd = $_[0];
  
  print "Do you want to flash y/n: ";
  chomp ($ask = <STDIN>);
  #$ask = "y";
  cmd_execute($flash_cmd) if ( $ask eq "y" );
  
  return ( $ask eq "y" );
}

sub print_title
{
  print "---------------------------------------------------------------------\n";
  print "$_[0] for $board\n";
  print "---------------------------------------------------------------------\n";
}

sub append_file
{
  my $log_file  = $_[0];
  my $temp_log  = $_[1];
  
  open(my $log_handle, ">>$log_file") or die "cannot open $log_file";
  open(my $temp_handle, $temp_log) or die "cannot open $temp_log";
  
  print $log_handle (@temp_content = <$temp_handle>);
  
  close($temp_handle);
  close($log_handle);
}

sub iar_flash_cmd
{
  $_[0] =~ /^(.+)\/(.+).ewp/; 
  my $debug_file = "$board/Exe/$2.out";

  my $arm_path = abs_path "$IAR_PATH/../../arm";
  my $bin_path = "$arm_path/bin";
  my $debugger_path = "$arm_path/config/debugger/NXP";
  
  my %mcu_para_hash = 
  ( # board => family (for macro), architecture, fpu, name
    'Board_EA4357'         => ['LPC18xx_LPC43xx', 'Cortex-M4', 'VFPv4', 'LPC4357_M4'       ],
    'Board_NGX4330'        => ['LPC18xx_LPC43xx', 'Cortex-M4', 'VFPv4', 'LPC4330_M4'       ],
    'Board_LPCXpresso1769' => ['LPC175x_LPC176x', 'Cortex-M3', 'None' , 'LPC1769'          ],
    'Board_LPCXpresso1347' => ['lpc1315'        , 'Cortex-M3', 'None' , 'LPC1347'          ],
    'Board_rf1ghznode'     => [''               , 'Cortex-M0', 'None' , 'LPC11U37FBD48_401'],
  );
  
  my @mcu_para = @{$mcu_para_hash{$board}};
  die "Board is not supported" unless @mcu_para;
    
  my $cmd = "cspybat \"$bin_path/armproc.dll\" \"$bin_path/armjlink.dll\" \"$debug_file\" --download_only --plugin \"$bin_path/armbat.dll\"";
  
  $cmd .= " --macro \"$debugger_path/Trace_$mcu_para[0].dmac\"" if $mcu_para[0];

  $repo_mcu_iar = "$repo_path/mcu/lpc43xx/iar";
  $cmd .= " --macro \"$repo_mcu_iar/lpc18xx_43xx_debug.mac\" --flash_loader \"$repo_mcu_iar/FlashLPC18xx_43xx_SPIFI.board\"" if $board eq 'Board_NGX4330';

  $cmd .= " --backend -B \"--endian=little\" \"--cpu=$mcu_para[1]\" \"--fpu=$mcu_para[2]\" \"-p\" \"$debugger_path/$mcu_para[3].ddf\" \"--semihosting\" \"--device=$mcu_para[3]\"";
  
  #SWD interface --> need change if use lpc43xx_m0
  $cmd .= " \"--drv_communication=USB0\" \"--jlink_speed=auto\" \"--jlink_initial_speed=1000\" \"--jlink_reset_strategy=0,0\" \"--jlink_interface=SWD\" \"--drv_catch_exceptions=0x000\" --drv_swo_clock_setup=72000000,0,2000000\"";
  
  $cmd .= " \"--jlink_script_file=$debugger_path/LPC4350_DebugCortexM4.JLinkScript\"" if $mcu_para[3] =~ /LPC43.._M4/;
 
  $cmd =~ s/\//\\/g;
  #print $cmd; die;
  return $cmd;
}