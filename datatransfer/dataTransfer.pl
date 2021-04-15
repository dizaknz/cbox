#! /usr/bin/perl

=head1 SYNOPSIS

  dataTransfer - Transfer data from one database to another. The term database
                 is used loosely here, basically any data source (CSV, Excel) 
                 for which a Perl database (DBD) driver exists

=head2 Options

  --help               Brief help message
  --config  <string>   Name of the configuration module to use for data transfer
  --trace   <string>   Enable tracing for a particular module

=cut


use strict;
use warnings;

use Carp;
use Getopt::Long;
use Pod::Usage;
use FindBin qw($Bin);
use Data::Dumper;

use lib "$Bin/lib";

use DataTransfer::Log;
use DataTransfer::Transfer;

# process command line options
my %opts = (help => sub { pod2usage(-verbose => 1) });

GetOptions(\%opts,
           "help|?",
           "config=s",
           "trace=s@",
          ) or pod2usage(-verbose => 1);

unless (exists $opts{config})
{
  croak pod2usage (-verbose => 1);
}  

# get logger
my $logger = DataTransfer::Log->get_logger();

foreach (@{$opts{trace}}) {
  $logger->debug ("Enabling tracing for: $_");
  DataTransfer::Log->get_logger($_)->level($DataTransfer::Log::TRACE);
}

my $configModule = "DataTransfer::Transfer::Config::$opts{config}";

eval "require $configModule";
if ($@) {
  $logger->fatal  ("$@");
  $logger->logdie ("Error loading $configModule");
}

# instantiate configuration object
my $config = $configModule->new();

eval {
  my $dataTransfer = DataTransfer::Transfer->new ($config);

  $logger->info ("Transferring data");
  $dataTransfer->transferData ();
};
if ($@) {
  $logger->fatal  ("$@");
  $logger->logdie ("Data transfer failed");
}

exit (0);
