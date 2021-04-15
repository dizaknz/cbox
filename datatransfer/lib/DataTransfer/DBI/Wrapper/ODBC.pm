package DataTransfer::DBI::Wrapper::ODBC;

sub dsn ($) {
  my ($self, $options) = @_;

  unless (exists $options->{driver} && exists $options->{database}) {
    DataTransfer::Log->get_logger()->logdie ("[ODBC] - Invalid database options: Require 'driver' and 'database'");
  }

  return "ODBC:$options->{database}";
}

1;
