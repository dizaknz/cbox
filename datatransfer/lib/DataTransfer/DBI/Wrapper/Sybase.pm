package DataTransfer::DBI::Wrapper::Sybase;

sub dsn ($) {
  my ($class, $options) = @_;

  unless ($options->{driver} && $options->{database}) {
    DataTransfer::Log->get_logger()->logdie ("[Sybase] - Invalid database options: Require 'driver' and 'database'");
  }

  my $dsn = "Sybase:database=$options->{database}";

  if ($options->{server}) {
    $dsn .= ";host=$options->{server}";
  }

  if ($options->{port}) {
    $dsn .= ";port=$options->{port}";
  }

  if ($options->{schema}) {
    $dsn .= ";schema=$options->{schema}";
  }

  return $dsn;
}

sub set_schema ($) {
  my ($self, $schema) = @_;

  # nothing to do, done in connection above
}

1;
