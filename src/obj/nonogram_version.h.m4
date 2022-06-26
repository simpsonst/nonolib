#ifndef nonogram_version_HEADER
#define nonogram_version_HEADER
dnl

#define nonogram_VERSION 0
#define nonogram_MINOR 0
#define nonogram_PATCHLEVEL 0

regexp(VERSION, `^v\([0-9]+\)\([^.]\)?\.\([0-9]+\)\([^.]\)?\.\([0-9]+\)[^-.]*\(-\([0-9]+\)-g\([0-9a-fA-F]+\)\)?$', ``
#undef nonogram_VERSION
#undef nonogram_MINOR
#undef nonogram_PATCHLEVEL
#define nonogram_VERSION \1
#define nonogram_MINOR \3
#define nonogram_PATCHLEVEL \5
'ifelse(`\7',,,``#define nonogram_AHEAD \7
'')`'ifelse(`\8',,,``#define nonogram_COMMIT \8
'')`'')

#endif
