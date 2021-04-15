package DataTransfer::DBI::Wrapper::Oracle;

sub dsn ($) {
  my ($class, $options) = @_;

  unless ($options->{driver} && $options->{server} && $options->{sid} && $options->{port}) {
    DataTransfer::Log->get_logger()->logdie (
      "[Oracle] - Invalid database options: Require 'driver', 'server', 'sid' and 'port'"
    );
  }

  return "Oracle:host=$options->{server};sid=" . $options->{sid} || $options->{database} . ";" .
         "port=$options->{port}";
}

1;
