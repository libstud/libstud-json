./: {*/ -build/ -pdjson/} doc{README.md LICENSE AUTHORS NEWS} manifest

# Don't install tests.
#
tests/: install = false
