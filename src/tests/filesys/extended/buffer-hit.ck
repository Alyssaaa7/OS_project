# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(buffer-hit) begin
(buffer-hit) create "newfoo"
(buffer-hit) open "newfoo"
(buffer-hit) reading "newfoo"
(buffer-hit) setting fd position to 0
(buffer-hit) reading "newfoo"
(buffer-hit) close "newfoo"
(buffer-hit) hit rate improves
(buffer-hit) end
EOF
pass;
