#!/usr/bin/env perl
#
# sudo yum install perl-Digest-CRC perl-Log-Log4perl
# cpan -i Class::Tiny
#
=pod

=head1 NAME

crcSearch.pl - searches a file for given CRC checksum

=head1 SYNOPSIS

crcSearch.pl -f <file> -q <CRC checksum> [-d -h]

=head1 ARGUMENTS

  --file    Binary file that CRC checksum orginated from
  --crc     Search for given checksum, hex
  --debug   Enable debug messages
  --help    Brief help message

=cut

use strict;
use warnings;
use Log::Log4perl qw(:easy);
use Log::Log4perl::Level;
use Getopt::Long;
use Pod::Usage;
use bignum;
no warnings 'portable';

# inject class
use lib sub {
=pod

=head1 CLASS

CRCSearch

=head1 DESCRIPTION

Simple search for CRC-64 ECMA-182 checksum in file

=cut
    package CRCSearch;

    use Digest::CRC;
    use bignum;
    no warnings 'portable';

    # ECMA polynomial 0x42F0E1EBA9EA3693 / 0xC96C5795D7870F42 / 0xA17870F5D4F51B49 
    use constant ECMA    => 0x42F0E1EBA9EA3693;
    # 8 bytes
    use constant WIDTH   => 64;
    # seed
    use constant INITVAL => 0xFFFFFFFFFFFFFFFF;
    # XOR all
    use constant XOROUT  => 0xFFFFFFFFFFFFFFFF;
    # no reversing bit order
    use constant REVIN   => 0;
    use constant REVOUT  => 0;
    use constant NBYTES  => 1;

    use Class::Tiny qw( found ), {
        context => Digest::CRC->new(
            width  => WIDTH, 
            init   => INITVAL, 
            xorout => XOROUT, 
            refout => REVOUT, 
            poly   => ECMA,
            refin  => REVIN,
            cont   => 1
        ),
        offsetRange => [0, 0]
    };

    # read file and search for CRC
    sub search () {
        my ($self, $file, $crc) = @_;
        my $buffer = undef;
        my @data = ();

        open(FILE, "<" . $file) or (main::FATAL ("Invalid file: " . $file) and die);
        binmode(FILE); 

        my ($digest, $readSize, $size, $fileSize) = (0, 0, 0, 0);

        seek(FILE, 0, 2);
        $fileSize = tell(FILE);
        seek(FILE, 0, 0);

        main::INFO ("Searching " . $file . " for CRC " . $crc . "\n");

        # linear search is not efficient at all, instead need to reverse
        # engineer the SEARCH CRC checksum and derive direct relationship
        # with byte offset in file and running CRC to allow binary search only
        # ever use checksums without thinking too much about them
        while (($readSize = read(FILE, $buffer, NBYTES))) {
            $size += $readSize;
            # call to digest clears out $ctx->{_data}, so use list to append buffers as file is read
            push @data, $buffer;
            my $ctx = $self->context;
            $ctx->add(join '', @data);
            $digest = $ctx->digest();

            main::DEBUG ("read: " . $size . " digest=" . $digest . "\n");

            # big num comparison
            if ($digest == $crc) {
                $self->offsetRange->[1] = $size;
                $self->found(1);
                last;
            }
        }
        main::INFO ("Read " . $size . " out of " . $fileSize . " bytes from " . $file);
        close(FILE);
    }

    sub reset() {
        my $self = shift;

        $self->context->reset();
        $self->offsetRange((0,0));
        $self->found(0);
    }
};

# main
my %opts = (help => sub { pod2usage(-verbose => 1) });

# defaults
my $file = "C_task.docx";
my $crc = 0x42DC2CEC8897C034;

GetOptions(\%opts,
           "help|?",
           "file=s",
           "crc=s",
           "debug",
          ) or pod2usage(-verbose => 1);

if (exists $opts{'debug'}) {
    Log::Log4perl->easy_init($DEBUG);
}
else {
    Log::Log4perl->easy_init($INFO);
}

my $logger = get_logger();

unless ($opts{'file'}) {
    WARN ("Missing input file, using default: " . $file);
}
else {
    $file = $opts{'file'};
}

unless ($opts{'crc'}) {
    WARN ("Missing CRC checksum, using default: " . $crc);
}
else {
    $crc = $opts{'crc'};
}

# start search
my $crcSearch = CRCSearch->new();

$crcSearch->search($file, $crc);

if ($crcSearch->found()) {
    my $range = $crcSearch->offsetRange();
    INFO (
        "Found CRC checksum: " . $crc . " file byte range: " . 
        $range->[0] . " to " . $range->[1]
    );
}
else {
    INFO ("Did not find CRC checksum: " . $crc . " for file:" . $file);
}

$crcSearch->reset();

exit(0);
