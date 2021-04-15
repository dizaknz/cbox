package DataTransfer::Transfer::Config::Sample;

=head1 NAME

DataTransfer::Transfer::Config::Sample - Sample configuration object for data transfer

=cut

use strict;
use warnings;

use base qw(DataTransfer::Transfer::Config);

=head2 CONFIGURATION

=over 

=item $sourceInfo

Source database connection properties, as parsed from INI file of format:

 [source_info]
 driver   =
 sid      =
 database = 
 server   =
 port     =
 schema   =
 user     =
 password =

=over 

=item driver

Type of datasource and DBD Perl database driver to use when connecting to data source

=item sid

Oracle only, it's system ID

=item database

Name of the database to connect to.

=item server

Name of database server that hosts the database

=item port

Port on the server that database instance is listening on for connections

=item schema

Database schema (in Oracle's case the database)

=item user

Database user that has import access to datasource

=item password

Password for above user

=back

=cut

my $sourceInfo = {
  driver   => undef,
  database => undef,
  server   => undef,
  port     => undef,
  schema   => undef,
  user     => undef,
  password => undef
};

=item $targetInfo

Target database connection properties, parsed from INI file of format:

 [target_info]
 driver   =
 database =
 server   =
 port     =
 schema   =
 user     =
 password =

=over 

=item driver

Type of datasource and DBD Perl database driver to use when connecting to data source, eg. a Postgres datasource

=item database

Name of the database to connect to.

=item server

Name of database server that hosts the database

=item port

Port on the server that database instance is listening on for connections

=item schema

Database schema (or namespace) that acts as default, in this case it's SIMS

=item user

Database user that has SELECT/INSERT/UPDATE access to target schema

=item password

Password for above user

=back

=cut

my $targetInfo = {
  driver   => undef,
  database => undef,
  server   => undef,
  port     => undef,
  schema   => undef,
  user     => undef,
  password => undef
};

=item $sourceQuery

Defines the list of SQL source and filter queries that provides the data to be translated, converted and inserted into
the target database. It consist of a list of dictionaries (hashes) that provide the following:

=over

=item query

The SQL query to execute on the source database

=item filter

Filter criteria, expressed as a dictionary which provides:

=over

=item query

The SQL query that provides the records that needs to be used in WHERE filter in above source query

=item columns

The columns in the source dataset that needs to be filtered by the data provided by above query

=item direct

The filter query is not a remote query, but the filter clause should just be appended to the local
source query (direct filter)

=back

=back

NOTE: 

The order of the source queries is important. When multiple queries target different tables, it must be done in the
correct order to satisfy referential integrity

=cut

my $sourceQuery = 
[
  {
    query  => undef,
    filter => undef
  }
];

=item $targetMapping

List of dictionaries (hashes) that provides the information needed to map source record fields to some output (target)
database table columns and the conversion to apply to each:

=over

=item table

The name of the target table to transfer data to

=item query

A reference to the correct source query definition in $sourceQuery, which will provide the columns needed in mapping
(below)

=item mapping

A list of dictionaries that provide the actual source to target column mapping needed:

=over

=item from

May be source column name or a reference to a Perl sub routine that provides some literal value or a SQL lookup query
that is bound to a source column using the format <column>, which translate the source value to a target value, eg. used
mostly for looking up foreign key IDs in lookup tables

=item to

The name of the target column that will receive the value extracted above

=item conversion

A Perl sub routine reference that provides the conversion routine that needs to be applied to the value provided before
inserting it into the target column

=back

=item search_columns

List of columns that unique identifies a record in the target table, if omitted it's all columns bar any auto generated
columns (below)

=item auto_columns

List of columns that are auto generated in database, eg serial keys or special columns that should never be considered to
uniquely identify a record and should never be updated

=back

=cut

my $targetMapping =
[
  {
    table   => undef,
    query   => \$sourceQuery->[0],
    mapping => [
                 {
                   from       => undef,
                   to         => undef,
                   conversion => undef
                 }
               ],
    search_columns => [ ],
    auto_columns   => [ ]
  }
];


sub new () {
  my $class = shift;
  my $self = $class->SUPER::new();

  $self->{source_info}    = $sourceInfo;
  $self->{target_info}    = $targetInfo;
  $self->{filter_info}    = undef,
  $self->{source_query}   = $sourceQuery,
  $self->{target_mapping} = $targetMapping,
  $self->{source_filter}  = undef;

  bless $self, $class;

  return $self;
}

=back

=cut


