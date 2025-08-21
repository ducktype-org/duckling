BAR256_JSON = """
{
    "metadata": {
        "authors": ["Patryk Rogalski"],
        "version": "2.5.6",
        "name": "bar",
        "license": "GLTWSPL",
        "description": ""
    },
    "dependencies": {
        "pkg1": {
            "version": ["2.3.6"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry_url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {}
        }
    },
    "dev_dependencies": {},
    "features": {},
    "targets": {},
    "profiles": {}
}
"""

FOO123_JSON = """
{
    "metadata": {
        "authors": ["Patryk Rogalski"],
        "version": "1.2.3",
        "name": "foo",
        "license": "GLTWSPL",
        "description": ""
    },
    "dependencies": {
        "pkg2": {
            "version": ["2.3.4"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry_url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {}
        },
        "pkg3": {
            "version": ["2.4.7"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry_url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {}
        }
    },
    "dev_dependencies": {},
    "features": {},
    "targets": {},
    "profiles": {}
}
"""

FOO125_JSON = """
{
    "metadata": {
        "authors": ["Patryk Rogalski"],
        "version": "1.2.5",
        "name": "foo",
        "license": "GLTWSPL",
        "description": ""
    },
    "dependencies": {},
    "dev_dependencies": {},
    "features": {},
    "targets": {},
    "profiles": {}
}
"""

FOO_MULTI = f'{{ "packages_metadata": [ {FOO123_JSON}, {FOO125_JSON} ] }} '
BAR_MUTLI = f'{{ "packages_metadata": [ {BAR256_JSON} ] }} '
