#!/usr/bin/perl
#
# bsh-dbus-parse-lacsv.pl -- Parse D-Bus traffic from logic analyzer CSV file
#
# quick'n'dirty, just basic parsing, ignores timing
#
# (C) 2025 Hajo Noerenberg
#
# Usage: bsh-dbus-parse-lacsv.pl <csv-file> [data row number]
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
use Digest::CRC;    # sudo apt install libdigest-crc-perl

open( IFILE, "<", $ARGV[0] ) || die("Can not open input file");

print STDERR "Header: " . <IFILE>;

my $row = defined( $ARGV[1] ) ? $ARGV[1] : 4;
my $dbusdata;

while ( my @fields = split( /,/, <IFILE> ) ) {
    $dbusdata .= chr( hex( $fields[$row] ) );
}

my $ctx = Digest::CRC->new(
    width  => 16,
    init   => 0x0000,
    xorout => 0x0000,
    refout => 0,
    poly   => 0x1021,
    refin  => 0,
    cont   => 0
);

my $lastdest = undef;

for ( my $i = 0 ; $i < length($dbusdata) ; ) {
    my $flen = unpack( "C", substr( $dbusdata, $i, 1 ) );
    if ( defined($lastdest)
        && ( $flen == ( 16 * int( $lastdest / 16 ) + 10 ) ) )
    {
        printf " | %02x (ack=ok)", $flen;
        $i++;
        $lastdest = undef;
        next;
    }

    $lastdest = undef;

    if ( $flen < 2 || $flen > 32 ) {
        printf
          "\nIgnoring byte 0x%02x at pos %d: frame exceeds min or max size",
          $flen, $i;
        $i++;
        next;
    }

    $ctx->reset();
    $ctx->add( substr( $dbusdata, $i, 4 + $flen ) );
    if ( $ctx->digest ) {
        printf "\nIgnoring byte 0x%02x at pos %d: crc checksum error", $flen, $i;
        $i++;
        next;
    }

    $lastdest = unpack( "C", substr( $dbusdata, $i + 1, 1 ) );

    printf "\n%02x | %02x.%02x-%02x | ",
      unpack( "C", substr( $dbusdata, $i + 0, 1 ) ),
      unpack( "C", substr( $dbusdata, $i + 1, 1 ) ),
      unpack( "C", substr( $dbusdata, $i + 2, 1 ) ),
      unpack( "C", substr( $dbusdata, $i + 3, 1 ) );
    for ( my $n = 4 ; $n < ( $flen + 2 ) ; $n++ ) {
        printf "%02x ", unpack( "C", substr( $dbusdata, $i + $n, 1 ) );
    }
    printf "| %02x %02x (crc=ok)",
      unpack( "C", substr( $dbusdata, $i + $flen + 2 + 0, 1 ) ),
      unpack( "C", substr( $dbusdata, $i + $flen + 2 + 1, 1 ) );

    $i += ( $flen + 4 );
}

print "\n";
