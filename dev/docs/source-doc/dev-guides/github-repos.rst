======================================
Duckling Github repositories explained
======================================

Here you can find description of all  active repositories in our project.

.. contents::
    :depth: 2
    :local:

``rift-dev``
============

The main repository of the project. Source code is located in the ``dev/`` directory. Setup is automated with `toolbox.py`.

Source code structure
---------------------

* ``DucklingLS`` - duckling language server code
* ``RiftCompiler`` - compiler code
* ``RiftVM`` - virtual machine code
* ``base`` - custom module with standard-library-like implmentations
* ``common`` - commonly used modules
* ``docs`` - documentation

``dev-space``
=============

This repository is a place to organize our workflow and keep materials that are not directly connected to the code.

Repository Structure
--------------------

Here you can find description of the most important parts of the repository.

* ``organizacja/``
    * ``dev/``
        * ``Q<n>/`` - Summary of the <n>th quarter of the project
        * ``plan.md`` - general developement schedule
        * ``scenariusz_weekly.md`` - weekly dev team meeting checklist
    * ``misje/`` - mission files
        * ``aktywne/`` - current missions
        * ``wstrzymane/`` - frozen missions
        * ``zakonczone/`` - finished mission
    * ``podsumowanie-dyskusji/`` - discussion notes
    * ``propozycje_misji/`` - mission suggestions
    * ``raport_gc/`` - ground control reports
    * ``website/`` - website notes and reports
    * ``zpp/`` - zpp notes and reports
    * ``organizacja.md`` - workflow description
    * ``osoby.md`` - contact info to (almost) all team members
* ``prace_naukowe/`` - zpp theses and other articles

Most information about current and finished tasks can be found in ``misje/`` directory 
(if you don't know what missions are, check out :doc:`work-organization` 
and `dev-space <https://github.com/ducktype-org/dev-space/blob/main/organizacja/orgranizacja.md>`_). 
Each mission directory has a ``info.md`` file with general mission description, 
``raporty/`` directory with progress reports and other files with notes and materials. 
Finished missions also have final report with mission summary.

``rift-doc``
============

This is the main duckling documentation repository, written for future duckling users. It describes how to write duckling code and how it works.
For instructions on how to build the documentation, check out `main README.md <https://github.com/ducktype-org/rift-doc>`_.

``website``
===========

In this repository you can find all of our website files. ``main`` branch represents current public version of the website (available at `ducktype.org <https://ducktype.org/>`_ and `duckling.pl <https://duckling.pl/>`_) and `dev` branch represents current stable development version (available at `testsite.ducktype.org <https://testsite.ducktype.org/>`_).

Repository setup
----------------

1. Download repository and setup git

.. code-block:: bash

	git clone git@github.com:ducktype-org/website.git

2. Setup virtual environment and python dependencies

.. code-block:: bash

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt

3. Setup node.js and its dependencies

.. code-block:: bash

    sudo apt install nodejs
    sudo apt install npm
    npm install
    npm install -g sass

4. Create `.env` file and set necessary enviroment variables

.. code-block:: 

    SECRET_KEY='secret key generated with django.core.management.utils.get_random_secret_key()'
    OLD_SECRET_KEY=''
    OLD_SECRET_KEY=''
    DEBUG='True'
    ALLOWED_HOSTS='*'
    STATIC_URL='static/'

5. To locally preview website run

.. code-block:: bash

    python3 ./website/manage.py runserver

``doc-confic``
==============

This repository is dedicated to configuration of all of our sphinx-based documentation. It is included as a submodule in ``rift-doc`` and ``rift-dev``.

``zpp``
=======

Each year some zpp teams join our organization. Since their projects are usually somewhat independent from core language developement, they work on a separete repository (usually fork of ``rift-dev``) and their work is later synced-up or merged into core repos. These repos are marked with ``-zpp`` suffix and are poject specific.
