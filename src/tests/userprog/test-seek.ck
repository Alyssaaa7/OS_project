# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(test-seek) begin
(test-seek) open "sample.txt" for verification
(test-seek) seek position 5 "sample.txt"
(test-seek) close "sample.txt"
(test-seek) end
test-seek: exit(0)
EOF
pass;
