===========
Init guards
===========

.. simple-description::

Intention of this library is to provide automatic way of detecting if initialization function caused cycle (either Panic should be thrown or reinit should be prevented), and to detect if initialization function was already called before.

.. important::
	Current implementation doesn't do that and should not be used until this module is corrected.

