package DataTransfer::DBI::Wrapper;

use strict;
use warnings;

use DBIx::AnyDBD;
use DataTransfer::Log;
use Data::Dumper;

use Exporter qw(import);
our @EXPORTS_OK = qw(connect);

sub connect($$) {
  my ($class, $options) = @_;

  my $logger = DataTransfer::Log->get_logger();

  unless (ref $options eq 'HASH') {
    $logger->logdie ("Database options must be a hash reference");
  }

  $logger->trace ('Connection options: ' . Dumper ($options));

  my $driver_class = $class . '::' . ucfirst($options->{driver});
  eval "require $driver_class";
  if ($@) {
    $logger->fatal("Could not require '$driver_class'");
    return;
  } else {
    if ($driver_class->can('pre_connect')) {
      $driver_class->pre_connect($options);
    } else {
      $logger->trace("No pre_connect in '$driver_class'");
    }
  }

  my $dsn = $driver_class->dsn($options);

  if ($dsn =~ /^dbi/i) {
    $logger->logdie ("Invalid DSN: $dsn, must omit DBI");
  }

  $logger->info("Connecting to $dsn");
  my $dbh = DBIx::AnyDBD->connect(
     'DBI:' . $dsn,
     $options->{user},
     $options->{password},
     {
        PrintWarn   => 1,
        PrintError  => 0,
        RaiseError  => 1,
        HandleError => sub { $logger->logdie(shift) },
        AutoCommit  => 1
    },
    $class
  );

  if( !$dbh ) {
    $logger->logdie("Connection to $dsn failed : $DBI::errstr"); 
  }

  # set default schema, if needed
  if ($options->{schema}) {
    $dbh->set_schema ($options->{schema});
  }

  # store away connection options 
  foreach my $key (keys %$options) {
    $dbh->set_option($key, $options->{$key});
  }
  $dbh->set_option('logger', $logger);

  $logger->info("Connected to " . $dbh->describe());
  return $dbh;
}

1;
