# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(buffer-coal) begin
(buffer-coal) create "foonew"
(buffer-coal) open "foonew"
(buffer-coal) writing "foonew"
(buffer-coal) setting fd position to 0
(buffer-coal) reading "foonew"
(buffer-coal) reasonable write count (about 128)
(buffer-coal) close "foonew"
(buffer-coal) end
EOF
pass;
