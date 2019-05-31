# This program is used to embed arbitrary data into a C binary. It takes
# a list of files as an input, and produces a .cpp data file that contains
# contents of all these files as collection of char arrays.
#
# Usage: perl <this_file> <file1> [file2, ...] > embedded_data.cpp

foreach my $i (1 .. $#ARGV) {
  open FD, '<:raw', $ARGV[$i] or die "Cannot open $ARGV[$i]: $!\n";
  printf("static const unsigned char v%d[] = {", $i);
  my $byte;
  my $j = 0;
  while (read(FD, $byte, 1)) {
    if (($j % 12) == 0) {
      print "\n";
    }
    printf '%#d,', ord($byte);
    $j++;
  }
  print "\n};\n";
  close FD;
}

print <<EOS;

#include "kiwi.h"
#include "web.h"

embedded_files_t 
EOS
printf("%s", $ARGV[0]);
print <<EOS;
[] = {
EOS

foreach my $i (1 .. $#ARGV) {
  print "  {\"$ARGV[$i]\", 0, v$i, sizeof(v$i)},\n";
}

print <<EOS;
  {NULL, 0, NULL, 0}
};
EOS
