==============================================================================
                                    TODO
==============================================================================
Unsorted TODO list of features and points of investigation that could be
useful for improving finit.

Runlevels
---------
   * Improve support for runlevels: startup, running (services from
     finit.conf), and shutdown as well as hooks when switching RL,
     startup at RL 1 and a default run level.
   * Implement initctl stop|start|restart|reload|status <SVC> and
     service <SVC> stop|start|restart|reload|status on top

Configuration
-------------
   * Add support for "init <q | reload-configuration>" and SIGHUP to
     reload finit.conf, but also
   * Add support for inotify to automatically reload finit.conf

Services and Tasks
------------------
   * Add support for "task [RUNLVL] ..." similar to services, but
     one-shot. Needs a trigger/when
   * Add support for service/task dependencies
   * Add support for "init list" to show running services

Miscellaneous
-------------
   * Add support for "init -v,--version | version"

Documentation
-------------

   * Write man pages for finit and finit.conf, steal from pimd man pages...
   * Update Debian finit.conf example with runlevels, tty/console and
     dependency handling

Investigation
-------------

   * Investigate FHS changes affecting runtime status etc., see
     http://askubuntu.com/questions/57297/why-has-var-run-been-migrated-to-run

..
.. Local Variables:
..  mode: rst
..  version-control: t
.. End:
