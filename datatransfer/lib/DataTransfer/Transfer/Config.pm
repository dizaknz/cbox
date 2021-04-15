package DataTransfer::Transfer::Config;

=head1 NAME

DataTransfer::Transfer::Config - Base class for configuration per data transfer

=head1 DESCRIPTION

This class describes each data transfer session, eg. source of data (database information), 
target database, source query and how to translate the source query into the target tables
and columns

=cut

use strict;
use warnings;

use Data::Dumper;

use DataTransfer::Log;

=head2 Methods

=over

=item C<new ($class)>

Instantiate empty DataTransfer::Transfer::Config class

=cut
sub new ($) {
  my $class = shift;

  # these are explicitly defined to document required fields by base class
  bless {
    source_info     => { 
                         driver   => undef,
                         database => undef,
                         server   => undef,
                         port     => undef,
                         schema   => undef,
                         user     => undef,
                         password => undef
                       },
    target_info    => {
                         driver   => undef,
                         database => undef,
                         server   => undef,
                         port     => undef,
                         schema   => undef,
                         user     => undef,
                         password => undef
                       }, 
    filter_info    => {
                         driver   => undef,
                         database => undef,
                         server   => undef,
                         port     => undef,
                         schema   => undef,
                         user     => undef,
                         password => undef
                       }, 
     source_query  => [            # list of SQL query scalar strings and filter string
                        { 
                          query  => undef,
                          filter => undef
                         },
                       ],
    target_mapping => [            # list of source_query column to db table column mappings
                        { 
                          table          => undef,     # table name
                          query          => undef,     # source query reference
                          search_columns => [ undef ], # list of column names that is the identifier for the record
                          auto_columns   => [ undef ],    # primary key columns, not to be updated during upsert
                          mapping        => [ 
                                              {
                                                from       => undef,  # source column in query or sub
                                                to         => undef,  # target column
                                                conversion => sub { } # conversion sub to apply to source column
                                              }
                                            ]
                         },
                       ],
    source_filter  => [              # list of filter columns and values
                        {
                          'column'   => undef,    # column name
                          'datatype' => undef,    # data type
                          'operator' => undef,    # operator, >=, IN, <=, ILIKE, etc.
                          'values'   => [ undef ] # filter values
                         },
                       ]
  }, $class;
}

my $logger = DataTransfer::Log::get_logger();

use constant {
  TRUE  => 1,
  FALSE => 0
};

=item C<isValid ()>

Rudimentary validation of the configuration object

=cut
sub isValid () {
  my $self = shift;

  my $valid = TRUE;

  # validate that source is hash ref with at least driver + name
  unless (! $self->{source_info} || ref $self->{source_info} eq 'HASH') {
    $logger->logerror ("Source DB Definition supplied is not valid hash reference");
    $logger->logerror (Dumper ($self->{source_info}));
    $valid = FALSE;
  }
   
  unless (! $self->{target_info} || ref $self->{target_info} eq 'HASH') {
    $logger->logerror ("Target DB Definition supplied is not valid hash reference");
    $logger->logerror (Dumper ($self->{target_info}));
    $valid = FALSE;
  }

  if ($self->{filter_info}) {
    unless (ref $self->{filter_info} eq 'HASH') {
      $logger->logerror ("Filter DB Definition supplied is not valid hash reference");
      $logger->logerror (Dumper ($self->{filter_info}));
      $valid = FALSE;
    }
  }

  unless (! $self->{source_query} || ref $self->{source_query} eq 'ARRAY') {
    $logger->logerror ("Query Definition supplied is not valid array reference");
    $logger->logerror (Dumper ($self->{source_query}));
    $valid = FALSE;
  }

  if ($self->{source_filter}) {
    unless (ref $self->{source_filter} eq 'ARRAY') {
      $logger->logerror ("Source Filter Definition supplied is not a valid array reference");
      $logger->logerror (Dumper ($self->{source_filter}));
      $valid = FALSE;
    }
  }

  return $valid;
}

=item C<sourceInfo()>

Setter/getter for data transfer source datasource connection information

=cut
sub sourceInfo() {
  my ($self, $source_info) = @_;

  if ($source_info) {
    unless (ref $source_info eq 'HASH') {
      $logger->logerror (Dumper ($source_info));
      $logger->logdie ("Source DB Definition supplied is not valid hash reference");
    }
    $self->{source_info} = $source_info;
  }
  return $self->{source_info};
}

=item C<targetInfo()>

Setter/getter for data transfer target datasource connection information

=cut
sub targetInfo() {
  my ($self, $target_info) = @_;

  if ($target_info) {
    unless (ref $target_info eq 'HASH') {
      $logger->logerror (Dumper ($target_info));
      $logger->logdie ("Target DB Definition supplied is not valid hash reference")
    }
    $self->{target_info} = $target_info;
  }
  return $self->{target_info};
}

=item C<filterInfo()>

Setter/getter for data transfer filter datasource connection information

=cut
sub filterInfo() {
  my ($self, $filter_info) = @_;

  if ($filter_info) {
    unless (ref $filter_info eq 'HASH') {
      $logger->logerror (Dumper ($filter_info));
      $logger->logdie ("Filter DB Definition supplied is not valid hash reference");
    }
    $self->{filter_info} = $filter_info;
  }
  return $self->{filter_info};
}

=item C<sourceQuery()>

Setter/getter for data transfer source query data structure

=cut
sub sourceQuery() {
  my ($self, $source_query) = @_;

  if ($source_query) {
    unless (ref $source_query eq 'ARRAY') {
      $logger->logerror (Dumper ($source_query));
      $logger->logdie ("Query Definition supplied is not valid array reference");
    }
    $self->{source_query} = $source_query;
  }
  return $self->{source_query};
}

=item C<targetMapping()>

Setter/getter for data transfer target mapping data structure

=cut
sub targetMapping() {
  my ($self, $target_mapping) = @_;

  if ($target_mapping) {
    unless (ref $target_mapping eq 'ARRAY') {
      $logger->logerror (Dumper ($target_mapping));
      $logger->logdie ("Mapping Definition supplied is not valid array reference");
    }
    $self->{target_mapping} = $target_mapping;
  }
  return $self->{target_mapping};
}

=item C<sourceFilter()>

Setter/getter for data transfer source filter data structure

=cut
sub sourceFilter() {
  my ($self, $source_filter) = @_;

  if ($source_filter) {
    unless (ref $source_filter eq 'ARRAY') {
      $logger->logerror (Dumper ($source_filter));
      $logger->logdie ("Source Filter Definition supplied is not a valid array reference");
    }
    $self->{source_filter} = $source_filter;
  }
  return $self->{source_filter};
}

=item C<initDatabaseConfigFromFile ($configfile)>

Init database configuration from external INI file. It parses INI style configuration file and
supports multiline parameters when quoted.

=cut
sub initDatabaseConfigFromFile ($) {
  my ($self, $configfile) = @_;

  my $section;
  my $params = {};
  my $multiline = 0;
  my ($key, $value);

  open FH, $configfile or die "Unable to open database configuration file: $configfile";

  while (<FH>) {
    # ignore comments and empty lines
    if (! m/^\s*$/ && ! m /^\s*#/) {
      if (m/^\[\D+\]$/) {
        if ($section) {
          # store away previous section
          if (exists $self->{$section}) {
            $self->{$section} = $params;
          }
          else {
            $logger->logwarn ("Discarding INI section: $section, not a valid DataTransfer::Transfer::Config field");
          }
          $section = undef;
          $params  = undef;
        }
        $section = $_;
        chomp ($section);
        $section =~ s/^\[//;
        $section =~ s/\]$//;
      }
      else {
        if ($section) {
          if ($multiline) {
            # inside, section, inside multiline params
            $value = $_;
            if (m/^"$/) {
              # end of multiline params
              $value =~ s/"$//;
              $multiline = 0;
            }
            # append to param
            $params->{$key} .= $value;
          }
          else {
            # inside section, parse params
            ($key, $value) = split (/=/);

            $key   =~ s/\s+//g;
            $value =~ s/\s+//g;

            if ($value =~ m/^"\D+"/) {
              $value =~ s/"//g;
              chomp ($value);
            }
            elsif ($value =~ m/^"/) {
              $value =~ s/^"//;
              $multiline = 1;
            }
            else {
              chomp ($value);
            }
            $params->{$key} = $value;
          }
        }
      }
    }
  }

  if ($section) {
    # store away last section as class field, only if it has been defined by new()
    if (exists $self->{$section}) {
      $self->{$section} = $params;
    }
    else {
      $logger->logwarn ("Discarding INI section: $section, not a valid DataTransfer::Transfer::Config field");
    }
    $section = undef;
  }

  close FH;
}

=back

=cut

1;
