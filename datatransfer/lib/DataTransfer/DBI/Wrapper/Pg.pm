package DataTransfer::DBI::Wrapper::Pg;

use Data::Dumper;

sub dsn ($) {
  my ($class, $options) = @_;

  unless ($options->{driver} && $options->{database}) {
    DataTransfer::Log->get_logger()->logdie ("[Postgres] - Invalid database options: Require 'driver' and 'database'");
  }

  my $dsn = "Pg:dbname=$options->{database}";

  if ($options->{server}) {
    $dsn .= ";host=$options->{server}"
  }

  if ($options->{port}) {
    $dsn .= ";port=$options->{port}"
  }

  return $dsn;
}

sub set_schema ($) {
  my ($self, $schema) = @_;

  $self->do ("SET search_path = '$schema', 'public'") or
  DataTransfer::Log->get_logger()->fatal ("Could not set default schema=$schema");
}

1;
