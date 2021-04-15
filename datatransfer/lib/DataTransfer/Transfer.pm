package DataTransfer::Transfer;

=head1 NAME

Transfer - inter-database data transfer

=cut

use strict;
use warnings;

use Carp;
use Data::Dumper;

use DataTransfer::DBI::Wrapper;
use DataTransfer::Log;

our $AUTOLOAD;

=head1 DESCRIPTION

Transfer data from one database to another, optionally filtered by results from a different data source. Each
transfer depends upon and understands a DataTransfer::Transfer::Config object.

=head2 Methods

=over 

=item C<new ($class)>

  Create data transfer object

=cut
sub new ($) {
  my ($class, $config) = @_;

  my $self = bless {
    configuration  => $config,
    logger         => undef,
    source_handle  => undef,
    filter_handle  => undef,
    target_handle  => undef,
    upsert_handles => {}
  }, $class;

  if ($config) {
    die "Invalid DataTransfer::Transfer::Config configuration object: " . Dumper ($config) unless $config->isa('DataTransferTransfer::Config');
    $self->init($config);
  }

  return $self;
}

=item C<init($self, $config)>

  Initialises a DataTransfer::Transfer object using mandatory DataTransferTransfer::Config object

=cut
sub init($$) {
  my ($self, $config) = @_;
  
  $self->configuration($config);

  $self->logger(DataTransfer::Log->get_logger());
  
  $self->logger()->logdie('Unable to initialise DataTransfer::Transfer without valid DataTransferTransfer::Config configuration')
  unless ref($self->configuration());
  
  $self->logger()->trace ("Connecting to databases");
  $self->initDBConnections();

  $self->logger()->trace ("Preparing upsert handles");
  $self->initUpsertHandles();
}

=item C<initDBConnections ($self)>

  Connects to all databases

=cut
sub initDBConnections ($) {
  my $self = shift;

  my $config = $self->configuration();
  my $logger = $self->logger();

  # connect to source database
  $self->source_handle(DataTransfer::DBI::Wrapper->connect($config->sourceInfo()) or $logger->logdie ("$!"));

  # connect to target database
  $self->target_handle(DataTransfer::DBI::Wrapper->connect($config->targetInfo()));
    
  if (!$self->target_handle()) {
    $self->source_handle()->disconnect();
    $logger->logdie ("$!");
  }

  if (defined $config->filterInfo() && $config->filterInfo()->{'database'}) {
    # connect to database that holds information needed to construct the filter clause
    $self->filter_handle(DataTransfer::DBI::Wrapper->connect($config->filterInfo()));
          
    if (!$self->filter_handle()) {
      $self->source_handle()->disconnect();
      $self->target_handle()->disconnect();
      $logger->fatal ("Cannot retrieve filter value clause needed for data transfer");
      $logger->logdie ("$!");
    }
  }
}

=item C<destroyDBConnections ($self)>

  Disconnects from all databases

=cut
sub destroyDBConnections ($) {
  my $self = shift;

  $self->source_handle()->disconnect() if $self->source_handle();
  $self->target_handle()->disconnect() if $self->target_handle();
  $self->filter_handle()->disconnect() if $self->filter_handle();
}

=item C<initUpsertHandles ($self)>

  Initialise statement handles for insert/update operations

=cut
sub initUpsertHandles ($) {
  my $self = shift;

  my $logger = $self->logger();
  my $config = $self->configuration();

  # call util helper function to extract SQL insert into hash key'd by target table
  my $stmts = $self->target_handle()->upsertStatements($config->targetMapping());

  $logger->trace ("Database statements: " . Dumper ($stmts));

  my %sths = ();
  foreach my $table (keys %$stmts) {
    my %handles = map {
                    $_ => $self->target_handle()->prepare ($stmts->{$table}->{$_});
                  } qw{select insert update};

    $sths{$table} = \%handles;

    # add field meta data (for error logging), unable to retrieve from statement
    # handle for insert/update action during upsert
    $sths{$table}->{fields} = $stmts->{$table}->{fields};
  }
  $self->upsert_handles(\%sths);
}

=item C<destroyUpsertHandles ($self)>

  Clean up database statement resources for insert/update operations

=cut
sub destroyUpsertHandles ($) {
  my $self   = shift;
  my $logger = $self->logger();

  foreach my $sth (values %{$self->upsert_handles()}) {
    $sth->{select}->finish() or $logger->error (DBI::errstr);
    $sth->{update}->finish() or $logger->error (DBI::errstr);
    $sth->{insert}->finish() or $logger->error (DBI::errstr);
  }
}

=item C<destroy ($self)>

  Cleans up all database resources

=cut
sub destroy ($) {
  my $self = shift;

  $self->logger()->trace("Cleaning up upsert statements");
  $self->destroyUpsertHandles();

  $self->logger()->trace("Cleaning up database connections");
  $self->destroyDBConnections();
}

# destructor, clean up resources
sub DESTROY ($) {
  my $self = shift;

  $self->destroy();

}

=item C<transferData ($self)>

  Executes the extract and filter SQL queries, using the output to perform upsert on the target database

=cut
sub transferData ($) {
  my $self = shift;

  my $config = $self->configuration();
  my $logger = $self->logger();

  my $sths = $self->upsert_handles();

  # count records inserted and updated for each table
  $self->target_handle()->{inserted} = {};
  $self->target_handle()->{updated}  = {};

  #
  # prepare column filter clause from $config->sourceFilter()
  # the column filter clause are made up of operator, a join and values clause
  my @columnFilters = ();

  foreach my $columnFilter (@{($config->sourceFilter() || [])}) {
    if ($columnFilter) {
      # ideally this should be abstracted as a query filter interface
      if ($columnFilter->{'datatype'} =~ /(CHAR|TEXT)/) {
        # some variant of character, escape the values
        foreach my $value (@{$columnFilter->{'values'}}) {
          $value = "'$value'" unless $value =~ /^'\S+'$/;
        }
      }
      elsif ($columnFilter->{'datatype'} eq 'DATE') {
        # convert string to date using SQL standard TO_DATE and ISO format
        foreach my $value (@{$columnFilter->{'values'}}) {
          $value = "TO_DATE ('$value', 'YYYY-MM-DD')" unless $value =~ /TO_DATE/;
        }
      }

      # ideally this should be abstracted as a query filter interface
      my $columnClause = '';
      if ($columnFilter->{'operator'} eq 'IN') {
        # eg. column IN (values1, values2)
        $columnClause = $columnFilter->{'column'} . ' ' . $columnFilter->{'operator'} . 
                        '(' . join (',', @{$columnFilter->{'values'}}) . ')';
      }
      elsif ($columnFilter->{'operator'}  =~ /^(<|>|<=|>=|LIKE|ILIKE|=~|=)$/) {
        # eg. column > value1 AND column > value2
        foreach my $value (@{$columnFilter->{'values'}}) {
          $columnClause .= ' AND ' if length ($columnClause) > 0;
          $columnClause .= $columnFilter->{'column'} . ' ' . $columnFilter->{'operator'} . ' ' . $value;
        }
      }
      else {
        $logger->logdie ("Invalid filter operator found: $columnFilter->{'operator'}");
      }
      push @columnFilters, $columnClause;
    }
  }

  # execute source query
  foreach my $sourceQuery (@{$config->sourceQuery()}) {
    if (!$sourceQuery->{query}) {
      $logger->warn ("Invalid source query found in configuration: " . Dumper ($sourceQuery) . ", skipping");
      next;
    }

    # filter clause to append to source query, if any
    my $filterClause = '';

    #
    # prepare the result set filter values either by appending a filter clause as specified
    # by user or by performing some filter query on some external data source and appending
    # the result set of the filter
    #
    if ($sourceQuery->{filter}) {
      my $filterQuery       = $sourceQuery->{filter}->{query};
      my $filterClauseCols  = $sourceQuery->{filter}->{columns};
      my $quoteFilterValues = $sourceQuery->{filter}->{quote_filter_values};
      my $filterQueryOnly   = $sourceQuery->{filter}->{filter_query_only};
      my $useDirectQuery    = $sourceQuery->{filter}->{direct};

      # check if an external data source filter query was supplied
      if (defined $filterQuery) {
        # check if we need apply the source column filter to the filter query
        if (!defined $filterQueryOnly || !$filterQueryOnly) {
          # append column clauses, if source filter provided
          if (scalar @columnFilters > 0) {
            # append if WHERE exist, else create WHERE clause
            if ($filterQuery =~ /WHERE/i && substr ($filterQuery, rindex ($filterQuery, 'WHERE')) !~ /\) (AS \w|\w)$/i) {
              $filterQuery .= ' AND ';
            }
            else {
              # add WHERE clause
              $filterQuery .= ' WHERE ';
            }
            $filterQuery .= join (' AND ', @columnFilters);
          }
        }

        if (!$filterClauseCols) {
          $logger->fatal("Filter query require filter join columns");
          # fail safe, move onto next source query
          next;
        }

        my $sthf = $self->filter_handle()->prepare($filterQuery);
        
        eval {
          $logger->trace ("Executing filter query: $filterQuery");
          $sthf->execute();
        };
        if ($@) {
          $logger->error ("Failed executing filter query: $filterQuery. Reason=" . $sthf->err());
          next;
        }

        # standard SQL syntax, ideally the filter clause should be a join with in-memory table
        # which is vendor specific
        $filterClause .= qq{
          WHERE (} . join (',', @$filterClauseCols) . qq{) IN (};

        my $i = 0;
        foreach my $filterRow (@{$sthf->fetchall_arrayref()}) {
          my $comma = $i ? ', ' : '';
          if (defined $quoteFilterValues && $quoteFilterValues == 1) {
            $filterClause .= "$comma('" . join ('','', @$filterRow) . "')";
          }
          else {
            $filterClause .= "$comma(" . join (',', @$filterRow) . ")";
          }
          $i++;
        }
        $filterClause .= ')';
        $sthf->finish();

        if (!$i) {
          $logger->info ("Nothing to upload, skipping");
          next;
        }
      }

      if (defined $useDirectQuery && $useDirectQuery == 1) {
        # direct filter query is just appended to the source query, it is a direct filter on 
        # the same data source
        if (scalar @columnFilters > 0) {
          my $sourceQuerySQL = $sourceQuery->{query} . $filterClause;

          if ($sourceQuerySQL =~ /WHERE/i && substr ($sourceQuerySQL, rindex ($sourceQuerySQL, 'WHERE')) !~ /\) (AS \w|\w)$/i) {
            $filterClause .= ' AND ';
          }
          else {
            $filterClause .= ' WHERE ';
          }
          $filterClause .= join (' AND ', @columnFilters);
        }
      }
    }

    # construct source query to execute 
    my $query = $sourceQuery->{query} . $filterClause;

    my $sourceSth = $self->source_handle()->prepare ($query) or $logger->error (DBI::errstr);

    eval {
      $logger->trace ("Executing source query: $query");
      $sourceSth->execute();
    };
    if ($@) {
      $logger->logdie ("Failed executing source query: $query\n $@");
    }

    my $rownu = 0;
    while (my $row = $sourceSth->fetchrow_hashref()) {
      $self->target_handle()->begin_work ();
      $self->target_handle()->{active_uow} = 1;

      $logger->trace (Dumper($row));

      eval {
        foreach my $targetMapping (@{$config->targetMapping()}) {
          # skip this table unless source query references match
          unless ($targetMapping->{query} eq \$sourceQuery) {
            next;
          }

          my $table = $targetMapping->{table};

          $self->target_handle()->upsert (
            $targetMapping,
            $sths->{$table},
            $row
          );
        }
        $self->target_handle()->commit();
      };
      if ($@) {
        # always roll back
        $self->target_handle()->rollback();

        if (! $self->target_handle()->get_option('on_error_continue')) {
          $logger->error ("Source query executed: $query\n $@");
          $logger->error ("Error occurred at record: $rownu of result set");
          $logger->fatal ("Transfer error has occurred, rolling back upsert");
          $sourceSth->finish();
          $logger->logdie("Transfer aborted");
        }
        else {
          # continue upon error to next row
          $logger->trace ("Ignoring error: $@");
        }
      }
      $self->target_handle()->{active_uow} = 0;
      $rownu++;
    }
    $sourceSth->finish();
  }
  # report some upsert statistics
  if ( ! $self->target_handle()->get_option ('dryrun') ) {
    foreach my $table (keys %{$self->target_handle()->{inserted}}) {
      $logger->info ("[$table] inserted " .  $self->target_handle()->{inserted}->{$table} . " records ");
    }
    foreach my $table (keys %{$self->target_handle()->{updated}}) {
      $logger->info ("[$table] updated " .  $self->target_handle()->{updated}->{$table} . " records ");
    }
  }

  1;
}

# setters and getters for class fields
sub AUTOLOAD {
  my $self = shift;
  my $type = ref($self) or croak "$self is not an object";

  my $name = $AUTOLOAD;
  $name =~ s/.*://;

  # make sure a getter/setter has been requested for an existing field
  unless ( exists $self->{$name}) {
    croak "Can't access `$name' field in class $type";
  }

  if (@_) {
    # setter
    return $self->{$name} = shift;
  } else {
    # getter
    return $self->{$name};
  }
}  

=pod

=back

=cut

1;
