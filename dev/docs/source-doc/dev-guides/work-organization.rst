=================
Work organization
=================

As our team and project keeps growing we implment some practicies to organize our workflow. This document is a summary of all you need to know.

.. contents::
    :depth: 1
    :local:

Team
====

    * ``core dev`` also known as ``dev``, is the main developement team. Dev team usually meets once a week on a ``weekly`` meeting to keep everyone up to date.
    * ``zpp`` are the teams of students from University of Warsaw, working on their thesis projects. They work somewhat separetly from core dev and usually report their updates on a ``global`` meeting, with the rest of the team.
    * In addition, there are other people with more miscellaneous responsibilities. This includes professonrs from UW, sharing their vast theoritical knowlage with us.

Tools
=====

Discord
-------

Discord server is our main way to contact each other. Online meeting are also hosted there. Here is a list of channels that deserve the most attention:

    * ``general`` - announcements important to all team members, most notibly ``global`` meeting dates.
    * ``praca`` - work related questions and discutions.
    * ``review-ping`` - announcements about new pull requests (and reminder to review them)
    * ``weekly`` - weekly meeting arragements, duscusions and notes.

GitHub
------

Our organization on GitHub is called ``ducktype-org``. Descriptions of all repositories can be found in :doc:`github-repos`.

GitHub projects
---------------

We also use GitHub projects to better organize our work. ``Mission Board`` and ``Ground control`` are kanban boards where you can find details about current tasks.

Google group (calendar)
-----------------------

Our google group is rarely used, we have it mostly to make calendar management easier. In calendar you can find check planned weekly and global meetings. 

Clockify
--------

Clockify is a time tracking tool, we use it to keep track of worked hours and make accounting easier.

Dev workflow summary
====================

    * Dev team meets once a week at a `weekly meeting <https://github.com/ducktype-org/dev-space/blob/main/organizacja/dev/scenariusz_weekly.md>`_ (dev only) and once a month at a global meeting (whole team).
    * Roughly once a moth at a weekly meeting a workplan for another month is made. The plan is posted in meeting notes and in `dev space <https://github.com/ducktype-org/dev-space/tree/main/organizacja/dev>`_ repository.
    * Each task (mission), have its own folder on `dev space <https://github.com/ducktype-org/dev-space/tree/main/organizacja/misje>`_. This folder containf ``info.md`` file with task description, ``raporty/`` directory with weekly update reports and other relevant files, notes etc. Additionaly finished misiions have a final report with mission summary.
    * Each week, before a weekly meeting a report must be submitted to each mission. Event when there was no progress, this information should be posted as a report so that other team member can easily acces it (for example if they were absent).
    * Finished missions recive a final report and are moved from ``aktywne/`` to ``zakonczone/``.
    * Each task should also have a card on a mission board (or GC kanban) and be linked to proper issues, branches, etc.
    * Tasks that are not a good fit for a mission are resolved by `Ground Control <https://github.com/ducktype-org/dev-space/blob/main/organizacja/orgranizacja.md#ground-control>`_.

See also
========
    * `Weekly meeting script <https://github.com/ducktype-org/dev-space/blob/main/organizacja/dev/scenariusz_weekly.md>`_
    * `DEV organization <https://github.com/ducktype-org/dev-space/blob/main/organizacja/orgranizacja.md>`_
