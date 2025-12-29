#!/usr/bin/perl
#
# timelight-extract-icons.pl -- Extract and decompress icons from B/S/H/ Timelight firmware
#
# just a quick'n'dirty hack, no error checking, no nothing
#
# (C) 2025 Hajo Noerenberg
#
# Usage: timelight-extract-icon.pl timelight-9001366847.bin
#
# sudo apt-get install netpbm
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

open( IF, "<", $ARGV[0] ) || die("Can not open input file");
binmode(IF);

my $mtable = 0xbf00;

for ( my $m = 0 ; $m < 25 ; $m++ ) {

    seek( IF, $mtable + $m * 4, 0 );

    my $mbuf;
    read( IF, $mbuf, 4 ) == 4 || die;
    my $itable = unpack( "V", $mbuf );
    my $mname  = sprintf "0x%08x", $itable;
    printf "Main table: %03d %s\n", $m, $mname;

    next if ( $itable > ( -s $ARGV[0] ) );

    seek( IF, $itable, 0 );
    read( IF, $mbuf, 4 ) == 4 || die;
    my $msig = unpack( "N", substr( $mbuf, 0, 4 ) );
    printf "%x\n", $msig;

    next if ( $msig != 0x40001 );

    # icon table

    my $l_icon = $itable;

    for ( my $i = 0 ; $i < 999 ; $i++ ) {

        seek( IF, $itable + 4 + $i * 4, 0 );

        my $ibuf;
        read( IF, $ibuf, 4 ) == (4) || die;

        my $icon = unpack( "V", $ibuf );

        #        next if ( $icon != 0x1a9dc );

        printf "%03d %08x: ", $i, $icon;

        if ( $icon == 0xffffffff ) {
            printf "\n";
            last;
        }

        seek( IF, $icon, 0 )      || die;
        read( IF, $ibuf, 3 ) == 3 || die;

        #  printf "0x%04x\n", $ibuf;

        my $sizex       = unpack( "C", substr( $ibuf, 0, 1 ) );
        my $sizey       = unpack( "C", substr( $ibuf, 1, 1 ) );
        my $compression = unpack( "C", substr( $ibuf, 2, 1 ) );

        my $bytesize = ( int( ( ( $sizex * $sizey ) + 8 ) / 16 ) + 1 ) * 2;

        printf "x=%02d y=%02d compression=%d bits=%d bytes=%d offdiff=%d\n", $sizex, $sizey, $compression,
          $sizex * $sizey, $bytesize, $icon - $l_icon;

        next if ( ( $sizex <= 1 ) && ( $sizey <= 1 ) );

        $l_icon = $icon;

        seek( IF, $icon + 3, 0 )                  || die;
        read( IF, $ibuf, $bytesize ) == $bytesize || die;

        my $bits = unpack "B*", $ibuf;

        if ( $compression == 1 ) {
            my $inbits = $bits;
            $bits = "";
            for ( my $k = 0 ; $k < length($inbits) ; $k += 8 ) {
                my $ctype   = substr( $inbits, $k + 0, 2 );
                my $payload = substr( $inbits, $k + 2, 6 );

                if ( $ctype eq "10" ) {
                    $bits .= $payload;
                }
                elsif ( $ctype eq "01" ) {
                    $bits .= "1" x oct( "0b" . $payload );
                }
                elsif ( $ctype eq "00" ) {
                    $bits .= "0" x oct( "0b" . $payload );
                }
                else {
                    die("Unknown chunk type: $ctype");
                }

                last if ( length($bits) >= ( $sizex * $sizey ) );
            }
        }

        my $fname = sprintf "%03d-%03d-%06x-%02d-%02d-%d", $m, $i, $icon, $sizex, $sizey, $compression;

        open( OF, ">", "/tmp/icon_tmp.pbm" ) || die;
        printf OF "P1\n%d %d\n%s\n", $sizex, $sizey, $bits;
        close(OF);

        my $debug = `pnmtopng /tmp/icon_tmp.pbm > timelight-icons/icon-$fname.png`;
        print $debug;

        last if ( $itable == 0xffffffff );
    }
}
