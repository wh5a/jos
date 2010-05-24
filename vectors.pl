#!/usr/bin/perl -w

# Based on xv6's vectors.pl
# Generate the trap/interrupt entry points,
# which needs to be pasted into kern/trapentry.S
# This isn't a nice hack, but does work in a simple way.

for(my $i = 0; $i < 256; $i++){
    if(($i < 8 || $i > 14) && $i != 17) {
        print "TRAPHANDLER_NOEC(vector$i,$i)\n";
    }
    else {
        print "TRAPHANDLER(vector$i,$i)\n";
    }
}

print "\n.data\n";
print ".globl vectors\n";
print "vectors:\n";
for(my $i = 0; $i < 256; $i++){
    print "  .long vector$i\n";
}
