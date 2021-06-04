# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(child_file) begin
(child_file) open "sample.txt"
(c-open-close) begin
(c-open-close) end
c-open-close: exit(0)
(child_file) wait(exec()) = 0
(child_file) close "sample.txt" in parent
(child_file) end
child_file: exit(0)
EOF
pass;
