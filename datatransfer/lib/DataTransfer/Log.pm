package DataTransfer::Log;

=head1 NAME DataTransfer::Log.pm

 Log.pm - Customised Log4Perl logger

=head2 Additions 
   
=over

=item init

 Init logger and load default configuration $log_conf

=item get_logger 

 init (if unitialised) and return logger

=back

=cut

use strict;
use warnings;
use Log::Log4perl;
use Log::Log4perl::Level;
use DataTransfer::Log::Logger;

our $log_conf = q(
  log4perl.rootLogger=DEBUG, SCREEN
  log4perl.appender.SCREEN=Log::Log4perl::Appender::Screen
  log4perl.appender.SCREEN.Threshold = TRACE
  log4perl.appender.SCREEN.layout=PatternLayout
  log4perl.appender.SCREEN.layout.cspec.N = sub { \
    my ($layout, $message, $category, $priority, $caller_level) = @_ ; \
    my %cvt = ( \
       'FATAL' => 'F_ERROR', \
       'ERROR' => 'ERROR', \
       'WARN'  => 'WARNING', \
       'INFO'  => 'INFO', \
       'DEBUG' => 'DEBUG', \
       'TRACE' => 'TRACE' ); \
    return $cvt{$priority}; \
  }
  log4perl.appender.SCREEN.layout.ConversionPattern=[%d] %-7N :%x %m%n 
 );

sub init {
  Log::Log4perl->init(\$log_conf);
  Log::Log4perl::NDC->push("");
}

sub get_logger {
  my ($self, $category) = @_;

  $self->init() unless Log::Log4perl->initialized();
  my $cat = defined($category) ? $category : caller;
  my $logger = Log::Log4perl->get_logger($cat);

  bless $logger, 'DataTransfer::Log::Logger';
}

1;
