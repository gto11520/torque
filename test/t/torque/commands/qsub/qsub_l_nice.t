#! /usr/bin/perl

use CRI::Test;
plan('no_plan');
use strict;
use warnings;
setDesc('Qsub -l nice');

#? apitest
#* This tests qsub -l nice=30


# Submit a job with qsub and get its job id
my %jobId = runCommandAs($props->get_property('moab.user.one'),'echo /bin/sleep 60 | qsub -l nice=30');
ok($jobId{'EXIT_CODE'} == 0,'Chekcing if qsub submission worked') or die("qsub failed with rc=$jobId{'EXIT_CODE'}");

# Run qstat -f on the submitted job and look for Resource_List.nice
my $nice = '';

# Untaint qsub output
my $jobId = $jobId{'STDOUT'};
$jobId = $1 if ($jobId =~ /(.*)/);
chomp($jobId);

my %qstat = runCommandAs($props->get_property('moab.user.one'),"qstat -f $jobId");

ok($qstat{'EXIT_CODE'} != 999,'Checking that qstat ran') or die('Couldn\'t run qstat');
my @stdout = join("\n",$qstat{'STDOUT'});
foreach my $line (@stdout)
{
   if ($line =~ /Resource_List.nice = (.*)/)
   {
      $nice = $1;
   }
}

die("Expected Resource_List.nice [30] but found [$nice]") unless cmp_ok($nice,'eq',"30",'Checking for Resource_List.nice [30]');

