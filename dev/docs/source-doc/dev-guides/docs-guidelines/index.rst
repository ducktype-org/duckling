========================
Documentation guidelines
========================

.. @TODO:
.. Write more tutorials

This is overview of all documentation in the Duckling project.

Documentation structure
=======================

Duckling documentation is divided into three parts:

* :ref:`duckling-doc <duckling-doc>` - user documentation, describing language funcionalities, usage, assumptions and goals 
* :ref:`source-doc <source-doc>` - developer documentation, guidelines and resources for developers
* :ref:`doxygen <doxygen>` - low level description of source code, implementation details and libraries usage

:ref:`source-doc <source-doc>` is further divided into:

* :doc:`dev-guides </source-doc/dev-guides/index>` - guidelines, tutorial and instructions
* :doc:`dev-handbook </source-doc/dev-handbook/index>` - high level description of source code

.. _duckling-doc:

Duckling documentation
======================

This is the user documentation, describing language funcionalities, usage, assumptions and goals.
You can find it in the `rift-doc <https://github.com/ducktype-org/rift-doc>`_ github repository 
and is currently under development.

.. _source-doc:

Source documentation
====================

You are currently in the source documentation. This part of the documentation is intended for developers contributing to Duckling.
It is in the main Duckling repository in the ``docs`` directory.

It is generated using Sphinx and reStructuredText (rst) format, for which you can find a quickstart guide here: :doc:`rst-quickstart`.

.. _doxygen:

Doxygen documentation
=====================

Doxygen documentation is generated from source code comments. It is intended for developers who want to understand the implementation details of Duckling.

A quick overview of Doxygen documentation can be found here: :doc:`doxygen-quickstart`. 

You can also write Markdown documentation that will be included in Doxygen, see :doc:`markdown-quickstart`.


.. toctree::
    :maxdepth: 1
    :caption: Read more:
    :glob:

    *

See also
========

*  :doc:`Printer examplary documentation (now inside Doxygen)`

* `Discord documentation channel <https://discord.com/channels/860531247826731029/1106545291852783627>`_
