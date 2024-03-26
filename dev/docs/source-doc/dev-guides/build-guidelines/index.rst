====================
Building the project
====================

.. contents::
    :depth: 2
    :local:

Standard build
==============

@TODO

Compiling with test coverage enabled
====================================

@TODO

Compiling with CCACHE enabled
=============================

.. note::
	For more details abould CCACHE see: `<https://ccache.dev/>`_


.. attention::
	CCACHE does not have 100% guarantee to never alter compilation result. Therefor it should not be used when full certainity is needed.


CCACHE is compiler cache that reuses object files if possible. During standard development it won't do much more then standard CMake. CCACHE can be much better then CMAKE when:
* Caching acros multiple build folders.
* Caching between checkouts.
* Caching compilation result from the past (useful when reverting and applying changes).

Is some cases having ccache enabled can be usefull.

Enabaling CCACHE
----------------

To enable CCACHE add following option to cmake:

.. code-block:: bash
	
	cmake -DUSE_CCACHE=ON ..


CCACHE configuration
--------------------

CCACHE is configured to:

* store its cache in :code:`dev/.ccache_cache` folder inside the repository.
* do not exeed 1GB.
