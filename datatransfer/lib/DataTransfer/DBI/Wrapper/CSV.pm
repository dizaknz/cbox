package DataTransfer::DBI::Wrapper::CSV;

sub dsn ($) {
  my ($class, $options) = @_;

  unless ($options->{driver} && $options->{database}) {
    DataTransfer::Log->get_logger()->logdie ("[CSV] - Invalid database options: Require 'driver' and 'database'");
  }

  # f_dir (file directory) is the database
  # f_ext (file extension) is always CSV
  # f_lock (file lock) is always exclusive
  # csv_class (CSV parser class) is always Text::CSV_XS
  #
  # the rest is derived from OS
  my $dsn = "CSV:f_dir=$options->{database};f_ext=.csv;f_lock=2;csv_class=Text::CSV_XS";

  return $dsn;
}

1;
