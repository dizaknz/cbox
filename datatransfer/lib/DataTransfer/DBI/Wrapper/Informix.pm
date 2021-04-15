package DataTransfer::DBI::Wrapper::Informix;

sub dsn ($) {
  my ($class, $options) = @_;

  unless ($options->{driver} && $options->{server} && $options->{database}) 
  {
    DataTransfer::Log->get_logger()->logdie ("[Informix] - Invalid database options: Require 'driver', 'server' and 'database'");
  }

  # Informix database connection depends on a mapping of $options->{server} to the
  # target server, protocol to use and the port to connect on, all the DSN needs
  # is these environmental variables to be setup and the name of the database
  # to connect to
  $ENV{'DBDATE'}           = 'DMY4/';
  $ENV{'DBCENTURY'}        = 'P';
  $ENV{'INFORMIXSERVER'}   = $options->{server};
  $ENV{'INFORMIXSQLHOSTS'} = "/opt/IBM/informix/etc/sqlhosts";

  my $dsn = "Informix:$options->{database}";

  return $dsn;
}

1;
