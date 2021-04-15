package DataTransfer::DBI::Wrapper::MSSQL;

# use ODBC with Sybase protocol
use base qw(DataTransfer::DBI::Wrapper::ODBC);

sub set_schema ($) {
  my ($self, $schema) = @_;

  # default is dbo for MSSQL 2005
}

1;
