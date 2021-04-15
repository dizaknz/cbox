package DataTransfer::Log::Logger;

use strict;
use warnings;

use Log::Log4perl::Logger;
use Log::Log4perl::NDC;
use Time::HiRes qw(time);
use Carp;

$Carp::Internal{ (__PACKAGE__) }++;

use base qw(Log::Log4perl::Logger);

my %log_count = ( fatal => 0 , error => 0, warn => 0 , info => 0, debug => 0, trace => 0 );
my $logger_start_time = time();

sub logdie {
  my $self = shift;
  my $message = shift;
  my ($module,$line)=((caller(0))[1],(caller(0))[2]);
  $message .= " in ".$module." at line ".$line;

  $log_count{fatal}++;
  $self->SUPER::fatal($message,@_);
  confess();
}

sub fatal {
  my $self = shift; 
  my $message = shift;
  my ($module,$line)=((caller(0))[1],(caller(0))[2]);
  $message .= " in ".$module." at line ".$line;

  $log_count{fatal}++;
  $self->SUPER::fatal($message,@_); 
}

sub error {
  my $self = shift; 
  my $message = shift;
  my ($module,$line)=((caller(0))[1],(caller(0))[2]);
  $message .= " in ".$module." at line ".$line;

  $log_count{error}++;
  $self->SUPER::error($message,@_); 
}

sub warn {
  my $self = shift; 

  $log_count{warn}++;
  $self->SUPER::warn(@_); 
}

sub fatal_count {
  my ($self,$inc) = @_;

  $log_count{fatal} += $inc if defined $inc; 
  return $log_count{fatal};
}

sub error_count {
  my ($self,$inc) = @_;

  $log_count{error} += $inc if defined $inc; 
  return $log_count{error};
}

sub warn_count {
  my ($self,$inc) = @_;

  $log_count{warn} += $inc if defined $inc; 
  return $log_count{warn};
}

sub logexit {
  my ($self,$msg) = @_;

  $msg = $0 if ! defined $msg;   
  my $fatals = $self->fatal_count(); 
  my $errors = $self->error_count(); 
  my $exit_code = 0;    
  if( $fatals > 0 ){
      $exit_code = 1;
  } elsif( $errors > 0 ){
      $exit_code = 1 + $errors;
      $exit_code = 255 if $exit_code > 255;
  } 
  if($exit_code != 0) {
      $self->SUPER::fatal($msg." finished with $fatals fatal errors and $errors errors" . 
                          sprintf (", took %0.2f seconds", time() - $logger_start_time)); 
  } else {
      $self->SUPER::info(sprintf($msg." finished, took %0.2f seconds",time() - $logger_start_time)); 
  }
  exit $exit_code;
}
  
1;
