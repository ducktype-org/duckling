=============
How to commit
=============

This page describes how to comiit changes to our repositories following good practicies and standards. Keep in mind that this tutorial is focused on ``rift-dev`` - our repisitory. Other repositories do not require as strict practicies.

.. contents::
    :depth: 1
    :local:

Repository setup
================

Set up your repo in a standard way. Some repositories have a ``toolbox.py`` script or setup instructions to facilitate the process.

Create a branch
===============

Most of our repositories don't allow commiting directly to the ``main`` branch. To make any changes you need to create a new feature branch. You can do that via GH issue or link a branch to an existing issue or kanban card (see ...).

Make some changes
=================

Don't forget to write tests and docs! On ``rift-dev`` Quacker bot will block the merge if coverage percantege drops (this can be bypassed if really needed).

Format your code
----------------

Before commiting changes, make sure that your code is properly formatted.
To do that you can use our bash script. From the `dev` directory run:

.. code-block:: bash

    ./scripts/formatting/format_repo.sh


Create pull request
===================

GH will automatically run tests, check coverage and do some other things to ensure quality. You also need to get at least one positive review (you can spam ``review-ping`` chanel on discord or scream at people at a weekly meeting to get your review faster). Make corrections until your change is accepted and passes all tests.

Merge changes
=============

Use ``squash and merge``. Try to provide meaningfull title and description.

Naming commits
==============

Because we use ``squash and merge`` commit names on a branch can be meaningless but we encaurage to use this naming style:
    * Your commit name: ``CommitType: short description``
    * If multiple commit types fit your commit, pick the best ones, and separate them with coma
      ``CommitType1, CommitType2: short description``
    * CommitTypes:
        * ``Add``- addition of code, docs, tests, etc
        * ``Refactor`` - refactor of code
        * ``Fix`` - fix of a bug/issue
        * ``Update`` - update of code, or other stuff due to time passing (new version, migration, etc)
        * ``Change`` - some change of code that doesn't fit three of the above
        * ``HotFix`` - hot fix of a bug/issue
        * ``Maintenance`` - repository maintenance -- related to git/GH
        * ``Delete``- deletion of some code, or other stuff
Merge into main should always be properly named.

Other repositories
==================

* On ``dev-space`` you can commit directly to ``main``. This repository serves purely organizational pourposes so quality of commits isn't that important.

* ``website`` repository have two branches with direct commits blocked - ``main`` and ``dev``. All chenges in this repository should be done on a separete feature branch, then merged into ``dev`` and then ``dev`` can be merged into ``main``.
