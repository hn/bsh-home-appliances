#!/usr/bin/perl
#
# bsh-dbus-logger.pl -- Parse D-Bus traffic
#
# quick'n'dirty, just basic parsing
#
# (C) 2025 Hajo Noerenberg
#
# Prerequisite: sudo apt install libdigest-crc-perl libdevice-serialport-perl
#
# Usage:
#   bsh-dbus-logger.pl <special character device>
#   bsh-dbus-logger.pl <binary log file>
#   bsh-dbus-logger.pl -c <data row number> <csv file>
#
# http://www.noerenberg.de/
# https://github.com/hn/bsh-home-appliances
#
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3.0 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
#

use strict;
use Getopt::Std;
use Digest::CRC;
use POSIX              qw(strftime);
use Time::HiRes        qw(gettimeofday);
use Device::SerialPort qw( :PARAM :STAT 0.07 );
use constant {
    MINMLEN  => 2,
    MAXMLEN  => 250,
    HDRLEN   => 2,
    CRCLEN   => 2,
    ACKLEN   => 1,
    RTIMEOUT => 1.2 * 2,
};

sub dbusparse {
    my ( $dbusdata, $waitformore, $prefix ) = @_;
    my $ok = 0;

    my $ctx = Digest::CRC->new(
        width  => 16,
        init   => 0x0000,
        xorout => 0x0000,
        refout => 0,
        poly   => 0x1021,
        refin  => 0,
        cont   => 0
    );

    my @errs;
    my $tail = length($dbusdata) - ( HDRLEN + MAXMLEN + CRCLEN + ACKLEN );
    my @tailerrs;
    for ( my $p = 0 ; $p < length($dbusdata) ; ) {
        my $len = unpack( "C", substr( $dbusdata, $p, 1 ) );

        if ( $len < MINMLEN || $len > MAXMLEN ) {
            push @{ ( $p < $tail ? \@errs : \@tailerrs ) },
              sprintf "%sIgnoring byte 0x%02x: frame exceeds min or max size",
              $prefix,
              $len;
            $p++;
            next;
        }

        $ctx->reset();
        $ctx->add( substr( $dbusdata, $p, HDRLEN + $len + CRCLEN ) );
        if ( $ctx->digest ) {
            push @{ ( $p < $tail ? \@errs : \@tailerrs ) },
              sprintf "%sIgnoring byte 0x%02x: crc checksum error",
              $prefix,
              $len;
            $p++;
            next;
        }

        while ( my $err = shift @errs ) {
            print $err . "\n";
        }
        while ( my $err = shift @tailerrs ) {
            print $err . "\n";
        }

        my $dest = unpack( "C", substr( $dbusdata, $p + 1, 1 ) );

        printf "%s%02x | %02x.%02x-%02x | ", $prefix,
          $len,
          $dest,
          unpack( "C", substr( $dbusdata, $p + HDRLEN + 0, 1 ) ),
          unpack( "C", substr( $dbusdata, $p + HDRLEN + 1, 1 ) );
        for ( my $n = 4 ; $n < ( $len + 2 ) ; $n++ ) {
            printf "%02x ", unpack( "C", substr( $dbusdata, $p + $n, 1 ) );
        }
        printf "| %02x %02x (crc=ok)",
          unpack( "C", substr( $dbusdata, $p + HDRLEN + $len + 0, 1 ) ),
          unpack( "C", substr( $dbusdata, $p + HDRLEN + $len + 1, 1 ) );

        $p += ( HDRLEN + $len + CRCLEN );

        my $ackornot = unpack( "C", substr( $dbusdata, $p, 1 ) );
        if ( ( $dest - $dest % 0x10 + 0xA ) == $ackornot ) {
            printf " | %02x (ack=ok)", $ackornot;
            $p++;
        }

        print "\n";

        $ok = $p;
    }

    while ( my $err = shift @errs ) {
        print $err . "\n";
    }

    if ( !$waitformore ) {
        while ( my $err = shift @tailerrs ) {
            print $err . "\n";
        }
    }

    return ( $ok > $tail ) ? $ok : $tail;
}

my %opt = ();
getopts( "c:", \%opt );

if ( -c $ARGV[0] ) {
    my $sp = new Device::SerialPort( $ARGV[0] )
      || die("Can't open serial port: $!");
    $sp->baudrate(9600);
    $sp->databits(8);
    $sp->parity("none");
    $sp->stopbits(1);
    $sp->handshake('none');
    $sp->write_settings;

    # Crude way to specify select() timeout
    # "Read_Total = read_const_time + (read_char_time * bytes_to_read)"
    $sp->read_const_time(RTIMEOUT);
    $sp->read_char_time(0);

    my $buf;
    while (1) {
        my ( $count, $raw ) = $sp->read(255);
        my ( $s,     $us )  = gettimeofday();
        my $ts = "[" . strftime( "%T", localtime($s) ) . sprintf( ".%06d", $us ) . "] ";
        if ( $count > 0 ) {
            $buf .= $raw;
            $buf = substr( $buf, dbusparse( $buf, 1, $ts ) );
        }
        elsif ( length($buf) > 0 ) {
            dbusparse( $buf, 0, $ts );
            $buf = "";
        }
    }

    $sp->close;
}
elsif ( $opt{c} ) {
    open( IFILE, "<", $ARGV[0] ) || die("Can not open CSV file: $!");

    print STDERR "Header: " . <IFILE>;

    my $buf;
    while ( my @fields = split( /,/, <IFILE> ) ) {
        $buf .= chr( hex( $fields[ $opt{c} ] ) );
    }

    dbusparse($buf);
    close(IFILE);
}
else {
    open( IFILE, "<:raw", $ARGV[0] ) || die("Can not open input file: $!");

    my $buf;
    while ( read( IFILE, my $raw, HDRLEN + MAXMLEN + CRCLEN + ACKLEN ) ) {
        $buf .= $raw;
    }

    dbusparse($buf);
    close(IFILE);
}
