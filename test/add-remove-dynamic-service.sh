#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")
export DEBUG=1

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
	texec rm -f /test_assets/service.sh
}

say "Hi, this is /run et al"
texec sh -c "echo 'ROOT:/bin'; ls -l /bin/; echo 'ROOT:/sbin'; ls -l /sbin/; echo 'ROOT:/run'; ls -l /run/; echo 'ROOT:/run/finit/'; ls -l /run/finit/; echo 'ROOT:/dev'; ls -l /dev/; ps; ls -l /proc/1/fd/"

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TENV_ROOT"/test_assets/

say "Add a dynamic service in $FINIT_CONF"
texec sh -c "echo 'service [2345] kill:20 log /test_assets/service.sh -- Dyn service' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.conf'
texec sh -c "echo > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 0 service.sh'
