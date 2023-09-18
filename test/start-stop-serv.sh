#!/bin/sh
set -eu

TEST_DIR=$(dirname "$0")

test_setup()
{
    say "Test start $(date)"
}

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF"
}

test_one()
{
    pidfn=$1
    service=$2

    say "Add service stanza '$service' to $FINIT_CONF ..."
    run "echo '$service' > $FINIT_CONF"

    say 'Reload Finit'
    #run "initctl debug"
    run "initctl reload"
    #run "initctl status"
    #run "ps"

    #run "initctl status"
    #run "ps"
    #run "initctl status serv"

    retry 'assert_num_children 1 serv'
    retry "assert_has_pidfile $pidfn" 1

    say 'Stop the service'
    run "initctl stop serv"

    retry 'assert_num_children 0 serv'

    say 'Start the service again'
    run "initctl start serv"

    retry 'assert_num_children 1 serv'

    say "Done, drop service from $FINIT_CONF ..."
    run "rm $FINIT_CONF"
    run "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

test_one "/run/serv.pid"  "service pid:!/run/serv.pid serv -- Forking service, type 1"
test_one "/run/serv.pid"  "service type:forking       serv -- Forking service, type 2"
# This one could never be started by and monitored by Finit: it forks to
# background and does not create a PID file.  Essentially it's lost to
# Finit, and any other sane process monitor.
#test_one "/run/serv.pid"  "service pid:/run/serv.pid  serv -p -- Forking service w/o PID file"
test_one "/run/serv.pid"  "service pid:/run/serv.pid  serv -n -- Foreground service w/o PID file"
test_one "/run/serv.pid"  "service                    serv -n -p -- Foreground service w/ PID file"
test_one "/run/servy.pid" "service pid:/run/servy.pid serv -n -p -P /run/servy.pid -- Foreground service w/ custom PID file"
