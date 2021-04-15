package DataTransfer::DBI::Wrapper::Mysql;

use Data::Dumper;

sub dsn ($) {
  my ($class, $options) = @_;

  unless (exists $options->{driver} && exists $options->{database}) {
    DataTransfer::Log->get_logger()->logdie ("[MySQL] - Invalid database options: Require 'driver' and 'database'");
  }

  my $dsn = "mysql:database=$options->{database}";

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

}

1;
